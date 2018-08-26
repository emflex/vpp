/*---------------------------------------------------------------------------
 * Copyright (c) 2016 Qosmos and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *---------------------------------------------------------------------------
 */

#ifndef __flowtable_impl_h__
#define __flowtable_impl_h__

#include <vppinfra/dlist.h>
#include <vppinfra/types.h>
#include <vppinfra/vec.h>
#include <vnet/ip/ip4_packet.h>

#include "flowtable.h"
#include "flowtable_tcp.h"

extern u64 flow_id;
extern flowtable_main_t flowtable_main;

typedef struct {
    u32 sw_if_index;
    u32 next_index;
    u32 offloaded;
} flow_trace_t;

clib_error_t *
flowtable_init(vlib_main_t * vm);

always_inline u64
hash_signature(flow_signature_t const * sig)
{
    if (flow_signature_is_ip4(sig))
    {
        return clib_xxhash(sig->s.ip4.src.as_u32 ^ sig->s.ip4.dst.as_u32
                ^ sig->s.ip4.proto ^ sig->s.ip4.port_src ^ sig->s.ip4.port_dst);
    } else {
        return clib_xxhash(sig->s.ip6.dst.as_u64[0] ^ sig->s.ip6.dst.as_u64[1]
                ^ sig->s.ip6.src.as_u64[0] ^ sig->s.ip6.src.as_u64[1]
                ^ sig->s.ip4.port_src ^ sig->s.ip4.port_dst);
    }
}

always_inline void
parse_ip4_packet(ip4_header_t * ip4, uword * is_reverse, struct ip4_sig * ip4_sig)
{
    ip4_sig->proto = ip4->protocol;

    if (ip4_address_compare(&ip4->src_address, &ip4->dst_address) < 0)
    {
        ip4_sig->src = ip4->src_address;
        ip4_sig->dst = ip4->dst_address;
        *is_reverse = 1;
    } else {
        ip4_sig->src = ip4->dst_address;
        ip4_sig->dst = ip4->src_address;
    }

    if (ip4_sig->proto == IP_PROTOCOL_UDP || ip4_sig->proto == IP_PROTOCOL_TCP)
    {
        /* tcp and udp ports have the same offset */
        udp_header_t * udp0 = (udp_header_t *) ip4_next_header(ip4);
        if (*is_reverse)
        {
            ip4_sig->port_src = udp0->src_port;
            ip4_sig->port_dst = udp0->dst_port;
        } else {
            ip4_sig->port_src = udp0->dst_port;
            ip4_sig->port_dst = udp0->src_port;
        }
    } else {
        ip4_sig->port_src = 0;
        ip4_sig->port_dst = 0;
    }
}

always_inline void
parse_ip6_packet(ip6_header_t * ip6, uword * is_reverse, struct ip6_sig * ip6_sig)
{
    ip6_sig->proto = ip6->protocol;

    if (ip6_address_compare(&ip6->src_address, &ip6->dst_address) < 0)
    {
        ip6_sig->src = ip6->src_address;
        ip6_sig->dst = ip6->dst_address;
        *is_reverse = 1;
    } else {
        ip6_sig->src = ip6->dst_address;
        ip6_sig->dst = ip6->src_address;
    }

    if (ip6_sig->proto == IP_PROTOCOL_UDP || ip6_sig->proto == IP_PROTOCOL_TCP)
    {
        /* tcp and udp ports have the same offset */
        udp_header_t * udp0 = (udp_header_t *) ip6_next_header(ip6);
        if (*is_reverse)
        {
            ip6_sig->port_src = udp0->src_port;
            ip6_sig->port_dst = udp0->dst_port;
        } else {
            ip6_sig->port_src = udp0->dst_port;
            ip6_sig->port_dst = udp0->src_port;
        }
    } else {
        ip6_sig->port_src = 0;
        ip6_sig->port_dst = 0;
    }
}

static inline u64
compute_packet_hash(vlib_buffer_t * buffer, uword * is_reverse, flow_signature_t * sig)
{
    ethernet_header_t * eth = (ethernet_header_t *)(buffer->data + buffer->current_data);

    if (PREDICT_TRUE(eth->type == clib_host_to_net_u16(ETHERNET_TYPE_IP6)
        || eth->type == clib_host_to_net_u16(ETHERNET_TYPE_IP4)))
    {
        vlib_buffer_advance(buffer, sizeof(ethernet_header_t));

        /* compute 5 tuple key so that 2 half connections
         * get into the same flow */
        if (PREDICT_TRUE(eth->type == clib_host_to_net_u16(ETHERNET_TYPE_IP4)))
        {
            sig->len = sizeof(struct ip4_sig);
            parse_ip4_packet(vlib_buffer_get_current(buffer),
                    is_reverse, (struct ip4_sig *) sig);
        } else if (eth->type == clib_host_to_net_u16(ETHERNET_TYPE_IP6)) {
            sig->len = sizeof(struct ip6_sig);
            parse_ip6_packet(vlib_buffer_get_current(buffer),
                    is_reverse, (struct ip6_sig *) sig);
        }

        return hash_signature(sig);
    }

    sig->len = 0;
    return 0;
}



always_inline timeout_msg_t *
timeout_msg_get(flowtable_main_t * fm)
{
    timeout_msg_t * msg = NULL;

    if (pthread_spin_lock(&fm->msg_lock) == 0)
    {
        msg = &fm->msg_pool[fm->last_msg_index];
        fm->last_msg_index = (fm->last_msg_index + 1) & TIMEOUT_MSG_MASK;
        pthread_spin_unlock(&fm->msg_lock);
    }

    return msg;
}

always_inline void
flow_entry_cache_fill(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt)
{
    int i;
    flow_entry_t * f;

    if (pthread_spin_lock(&fm->flows_lock) == 0)
    {
        if (PREDICT_FALSE(fm->flows_cpt > fm->flows_max)) {
            pthread_spin_unlock(&fm->flows_lock);
            return;
        }

        for (i = 0; i < FLOW_CACHE_SZ; i++)
        {
            pool_get_aligned(fm->flows, f, CLIB_CACHE_LINE_BYTES);
            vec_add1(fmt->flow_cache, f - fm->flows);
        }
        fm->flows_cpt += FLOW_CACHE_SZ;

        pthread_spin_unlock(&fm->flows_lock);
    }
}

always_inline void
flow_entry_cache_empty(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt)
{
    int i;

    if (pthread_spin_lock(&fm->flows_lock) == 0)
    {
        for (i = vec_len(fmt->flow_cache) - 1; i > FLOW_CACHE_SZ; i--)
        {
            u32 f_index = vec_pop(fmt->flow_cache);
            pool_put_index(fm->flows, f_index);
        }
        fm->flows_cpt -= FLOW_CACHE_SZ;

        pthread_spin_unlock(&fm->flows_lock);
    }
}

always_inline flow_entry_t *
flow_entry_alloc(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt)
{
    u32 f_index;
    flow_entry_t * f;

    if (vec_len(fmt->flow_cache) == 0)
        flow_entry_cache_fill(fm, fmt);

    if (PREDICT_FALSE((vec_len(fmt->flow_cache) == 0)))
        return NULL;

    f_index = vec_pop(fmt->flow_cache);
    f = pool_elt_at_index(fm->flows, f_index);

    return f;
}

always_inline void
flow_entry_free(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt, flow_entry_t * f)
{
    vec_add1(fmt->flow_cache, f - fm->flows);

    if (vec_len(fmt->flow_cache) > 2 * FLOW_CACHE_SZ)
        flow_entry_cache_empty(fm, fmt);
}

always_inline void
flowtable_entry_remove(flowtable_main_per_cpu_t * fmt, flow_entry_t * f)
{
    /* remove node from hashtable */
    clib_dlist_remove(fmt->ht_lines, f->ht_index);
    pool_put_index(fmt->ht_lines, f->ht_index);

    /* if list is empty, free it and delete hashtable entry */
    if (dlist_is_empty(fmt->ht_lines, f->ht_line_index))
    {
        pool_put_index(fmt->ht_lines, f->ht_line_index);

        BVT(clib_bihash_kv) kv = {.key = f->sig_hash};
        BV(clib_bihash_add_del) (&fmt->flows_ht, &kv, 0  /* is_add */);
    }
}

static inline void
queue_expiration_message(flowtable_main_t * fm, u32 ctx_id, flow_stats_t * stats)
{
    timeout_msg_t * msg;

    /* if ctx_id is unset, there is no flow to attach the stats to */
    if (ctx_id == 0)
        return;

    msg = timeout_msg_get(fm);
    if (PREDICT_FALSE(msg == NULL))
        return;

    msg->flags = 1;
    msg->ctx_id = ctx_id;
    msg->clt_pkts = stats[0].pkts;
    msg->srv_pkts = stats[1].pkts;
    msg->clt_Bytes = stats[0].Bytes;
    msg->srv_Bytes = stats[1].Bytes;

    if (PREDICT_FALSE(fm->first_msg_index == ~0))
        fm->first_msg_index = fm->last_msg_index;
}

always_inline void
expire_single_flow(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt,
        flow_entry_t * f, dlist_elt_t * e)
{
    ASSERT(f->timer_index == (e - fmt->timers));
    queue_expiration_message(fm, f->infos.data.ctx_id, (flow_stats_t *) &f->stats);

    /* timers unlink */
    clib_dlist_remove(fmt->timers, e - fmt->timers);
    pool_put(fmt->timers, e);

    /* hashtable unlink */
    flowtable_entry_remove(fmt, f);

    /* free to flow cache && pool (last) */
    flow_entry_free(fm, fmt, f);
}

always_inline u64
flowtable_timer_expire(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt,
    u32 now)
{
    u64 expire_cpt;
    flow_entry_t * f;
    u32 * time_slot_curr_index;
    dlist_elt_t * time_slot_curr;
    u32 index;

    time_slot_curr_index = vec_elt_at_index(fmt->timer_wheel, fmt->time_index);

    if (PREDICT_FALSE(dlist_is_empty(fmt->timers, *time_slot_curr_index)))
        return 0;

    expire_cpt = 0;
    time_slot_curr = pool_elt_at_index(fmt->timers, *time_slot_curr_index);

    index = time_slot_curr->next;
    while (index != *time_slot_curr_index && expire_cpt < TIMER_MAX_EXPIRE)
    {
        dlist_elt_t * e = pool_elt_at_index(fmt->timers, index);
        f = pool_elt_at_index(fm->flows, e->value);

        index = e->next;
        expire_single_flow(fm, fmt, f, e);
        expire_cpt++;
    }

    return expire_cpt;
}

always_inline void
timer_wheel_insert_flow(flowtable_main_per_cpu_t * fmt, flow_entry_t * f)
{
    u32 timer_slot_head_index;

    timer_slot_head_index = (fmt->time_index + f->lifetime) % TIMER_MAX_LIFETIME;
    clib_dlist_addtail(fmt->timers, timer_slot_head_index, f->timer_index);
}

always_inline void
timer_wheel_resched_flow(flowtable_main_per_cpu_t * fmt, flow_entry_t * f, u32 now)
{
    clib_dlist_remove(fmt->timers, f->timer_index);
    f->expire = now + f->lifetime;
    timer_wheel_insert_flow(fmt, f);

    return;
}

static void
recycle_flow(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt, u32 now)
{
    u32 next;

    next = (now + 1) % TIMER_MAX_LIFETIME;
    while (PREDICT_FALSE(next != now))
    {
        flow_entry_t * f;
        u32 * slot_index = vec_elt_at_index(fmt->timer_wheel, next);

        if (PREDICT_FALSE(dlist_is_empty(fmt->timers, *slot_index))) {
            next = (next + 1) % TIMER_MAX_LIFETIME;
            continue;
        }
        dlist_elt_t * head = pool_elt_at_index(fmt->timers, *slot_index);
        dlist_elt_t * e = pool_elt_at_index(fmt->timers, head->next);

        f = pool_elt_at_index(fm->flows, e->value);
        return expire_single_flow(fm, fmt, f, e);
    }

    /*
     * unreachable:
     * this should be called if there is no free flows, so we're bound to have
     * at least *one* flow within the timer wheel (cpu cache is filled at init).
     */
    clib_error("recycle_flow did not find any flow to recycle !");
}

/* TODO: replace with a more appropriate hashtable */
static inline flow_entry_t *
flowtable_entry_lookup_create(flowtable_main_t * fm, flowtable_main_per_cpu_t * fmt,
    BVT(clib_bihash_kv) * kv, flow_signature_t const * sig, u32 const now, int * created)
{
    flow_entry_t * f;
    dlist_elt_t * ht_line;
    dlist_elt_t * timer_entry;
    dlist_elt_t * flow_entry;
    u32 ht_line_head_index;

    ht_line = NULL;

    if (PREDICT_FALSE(kv->key == 0))
        return NULL;

    /* get hashtable line */
    if (PREDICT_TRUE(BV(clib_bihash_search) (&fmt->flows_ht, kv, kv) == 0))
    {
        ht_line_head_index = (u32) kv->value;
        ht_line = pool_elt_at_index(fmt->ht_lines, ht_line_head_index);
        u32 index;

        /* The list CANNOT be a singleton */
        index = ht_line->next;
        while (index != ht_line_head_index)
        {
            dlist_elt_t * e = pool_elt_at_index(fmt->ht_lines, index);
            f = pool_elt_at_index(fm->flows, e->value);
            if (PREDICT_TRUE(memcmp(&f->sig, sig, sig->len) == 0))
                return f;

            index = e->next;
        }
    } else {
        /* create a new line */
        pool_get(fmt->ht_lines, ht_line);

        ht_line_head_index = ht_line - fmt->ht_lines;
        clib_dlist_init(fmt->ht_lines, ht_line_head_index);
        kv->value = ht_line_head_index;
        BV(clib_bihash_add_del) (&fmt->flows_ht, kv, 1  /* is_add */);
    }

    /* create new flow */
    f = flow_entry_alloc(fm, fmt);
    if (PREDICT_FALSE(f == NULL)) {
        recycle_flow(fm, fmt, now);
        f = flow_entry_alloc(fm, fmt);
        if (PREDICT_FALSE(f == NULL))
            clib_error("flowtable failed to recycle a flow");
    }

    *created = 1;
    f->infos.data.flow_id = ++flow_id;

    memset(f, 0, sizeof(*f));
    f->sig.len = sig->len;
    clib_memcpy(&f->sig, sig, sig->len);
    f->sig_hash = kv->key;
    f->lifetime = TIMER_DEFAULT_LIFETIME;
    f->expire = now + TIMER_DEFAULT_LIFETIME;

    /* insert in timer list */
    pool_get(fmt->timers, timer_entry);
    timer_entry->value = f - fm->flows;  /* index within the flow pool */
    f->timer_index = timer_entry - fmt->timers;  /* index within the timer pool */
    timer_wheel_insert_flow(fmt, f);

    /* insert in ht line */
    pool_get(fmt->ht_lines, flow_entry);
    f->ht_index = flow_entry - fmt->ht_lines;  /* index within the ht line pool */
    flow_entry->value = f - fm->flows;  /* index within the flow pool */
    f->ht_line_index = ht_line_head_index;
    clib_dlist_addhead(fmt->ht_lines, ht_line_head_index, f->ht_index);

    return f;
}

static inline void
timer_wheel_index_update(flowtable_main_per_cpu_t * fmt, u32 now)
{
    u32 new_index = now % TIMER_MAX_LIFETIME;

    if (PREDICT_FALSE(fmt->time_index == ~0))
    {
        fmt->time_index = new_index;
        return;
    }

    if (new_index != fmt->time_index)
    {
        /* reschedule all remaining flows on current time index
         * at the begining of the next one */

        u32 * curr_slot_index = vec_elt_at_index(fmt->timer_wheel, fmt->time_index);
        dlist_elt_t * curr_head = pool_elt_at_index(fmt->timers, *curr_slot_index);

        u32 * next_slot_index = vec_elt_at_index(fmt->timer_wheel, new_index);
        dlist_elt_t * next_head = pool_elt_at_index(fmt->timers, *next_slot_index);

        if (PREDICT_FALSE(dlist_is_empty(fmt->timers, *curr_slot_index)))
        {
            fmt->time_index = new_index;
            return;
        }

        dlist_elt_t * curr_prev = pool_elt_at_index(fmt->timers, curr_head->prev);
        dlist_elt_t * curr_next = pool_elt_at_index(fmt->timers, curr_head->next);

        /* insert timer list of current time slot at the begining of the next slot */
        if (PREDICT_FALSE(dlist_is_empty(fmt->timers, *next_slot_index)))
        {
            next_head->next = curr_head->next;
            next_head->prev = curr_head->prev;
            curr_prev->next = *next_slot_index;
            curr_next->prev = *next_slot_index;
        } else {
            dlist_elt_t * next_next = pool_elt_at_index(fmt->timers, next_head->next);
            curr_prev->next = next_head->next;
            next_head->next = curr_head->next;
            next_next->prev = curr_head->prev;
            curr_next->prev = *next_slot_index;
        }

        /* reset current time slot as an empty list */
        memset(curr_head, 0xff, sizeof(*curr_head));

        fmt->time_index = new_index;
    }
}

always_inline int
flow_tcp_update_lifetime(flow_entry_t * f, tcp_header_t * hdr)
{
    tcp_state_t old_state, new_state;

    ASSERT(f->tcp_state < TCP_STATE_MAX);

    old_state = f->tcp_state;
    new_state = tcp_trans[old_state][tcp_event(hdr)];

    if (old_state != new_state)
    {
        f->tcp_state = new_state;
        f->lifetime = tcp_lifetime[new_state];
        return 1;
    }

    return 0;
}

always_inline int
flow_update_lifetime(flow_entry_t * f, vlib_buffer_t * buffer)
{
    /*
     * XXX: we already skipped the ethernet header
     * CHECK-ME: assert we have enough wellformed data to read the tcp header.
     */
    if (f->sig.len == sizeof(struct ip4_sig))
    {
        vlib_buffer_advance(buffer, sizeof(ip4_header_t));

        if (f->sig.s.ip4.proto == IP_PROTOCOL_TCP) {
            return flow_tcp_update_lifetime(f, vlib_buffer_get_current(buffer));
        }
    } else if (f->sig.len == sizeof(struct ip6_sig))
    {
        vlib_buffer_advance(buffer, sizeof(ip6_header_t));
        if (f->sig.s.ip6.proto == IP_PROTOCOL_TCP) {
            return flow_tcp_update_lifetime(f, vlib_buffer_get_current(buffer));
        }
    }

    return 0;
}

#endif  /* __flowtable_impl_h__ */
