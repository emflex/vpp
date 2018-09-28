/*
 * dpi.h - 3GPP TS 29.244 UPF DPI header file
 *
 * Copyright (c) 2017 Travelping GmbH
 *
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
 */
#ifndef __included_upf_dpi_h__
#define __included_upf_dpi_h__

#include <stddef.h>
#include <upf/upf.h>

#define UPF_DPI_APPLICATION_NAME_LEN_MAX 64

typedef struct {
  /* App index */
  u32 index;
  /* Regex expression */
  regex_t rule;
} upf_dpi_args_t;

int upf_dpi_add_multi_regex(upf_dpi_args_t * args, u32 * db_index);
int upf_dpi_lookup(u32 db_index, u8 * str, uint16_t length, u32 * app_index);
int upf_dpi_remove(u32 db_index);
int upf_dpi_get_db_contents(u32 db_index, regex_t ** expressions, u32 ** ids);
int upf_app_add_del (upf_main_t * sm, u8 * name, int add);
int upf_rule_add_del (upf_main_t * sm, u8 * name, u32 id,
                      int add, upf_rule_args_t * args);
void foreach_upf_flows (BVT (clib_bihash_kv) * kvp, void * arg);

int upf_dpi_get_db_id(u8 * app_name, u32 * path_db_index, u32 * host_db_index);

#define MIN(x,y) (((x)<(y))?(x):(y))

always_inline int
upf_dpi_parse_ip4_packet(ip4_header_t * ip4, u32 path_db_id,
                         u32 host_db_id, u32 * app_index)
{
  int tcp_payload_len = 0;
  tcp_header_t *tcp = NULL;
  u8 *http = NULL;
  u8 *version = NULL;
  u8 *host = NULL;
  u8 *host_end = NULL;
  u16 uri_length = 0;
  u16 host_length = 0;
  int res = 0;
  u32 path_app_index = ~0;
  u32 host_app_index = ~0;

  if (path_db_id == ~0)
    return -1;

  if (host_db_id == ~0)
    return -1;

  if (ip4->protocol != IP_PROTOCOL_TCP)
    return -1;

  tcp = (tcp_header_t *) ip4_next_header(ip4);

  tcp_payload_len = clib_net_to_host_u16(ip4->length) -
                    sizeof(ip4_header_t) - tcp_header_bytes(tcp);

  if (tcp_payload_len < 8)
    return -1;

  http = (u8*)tcp + tcp_header_bytes(tcp);

  if ((http[0] != 'G') ||
      (http[1] != 'E') ||
      (http[2] != 'T'))
    {
      return -1;
    }

  http += sizeof("GET");
  tcp_payload_len -= sizeof("GET");

  version = (u8*)strchr((const char*)http, ' ');
  if (version == NULL)
    return -1;

  uri_length = version - http;

  res = upf_dpi_lookup(path_db_id, http,
                       MIN(uri_length, tcp_payload_len),
                       &path_app_index);

  if ((res < 0) || (path_app_index == ~0))
    return -1;

  host = (u8*)strstr((const char*)http, "Host");
  if (host == NULL)
    return -1;

  host_end = (u8*)strchr((const char*)host, '\r');
  if (host_end == NULL)
    return -1;

  host_length = host_end - host;

  res = upf_dpi_lookup(host_db_id, host,
                       MIN(host_length, tcp_payload_len),
                       &host_app_index);

  if ((res < 0) || (host_app_index == ~0))
    return -1;

  if (path_app_index != host_app_index)
    return -1;

  *app_index = host_app_index;

  return 0;
}

always_inline upf_pdr_t *
upf_get_highest_dpi_pdr (struct rules * active, int direction)
{
  upf_pdr_t *pdr = NULL;
  upf_pdr_t *pdr_iter = NULL;
  int iter_direction = 0;

  if (vec_len(active->pdr) == 0)
    return NULL;

  vec_foreach (pdr_iter, active->pdr)
    {
      if (!pdr_iter->app_name)
        continue;

      iter_direction = (pdr_iter->pdi.src_intf == SRC_INTF_ACCESS) ? UL_SDF : DL_SDF;
      if (iter_direction != direction)
        continue;

      if (pdr == NULL)
        {
          pdr = pdr_iter;
          continue;
        }

      if (pdr_iter->precedence < pdr->precedence)
        pdr = pdr_iter;
    }

  return pdr;
}

always_inline void
upf_update_flow_app_index (flow_entry_t * flow, upf_pdr_t * pdr,
                           u8 * pl, int is_ip4)
{
  if (!flow)
    return;

  if (flow->app_index != ~0)
    return;

  if (is_ip4)
    {
      if (pdr->app_name != NULL)
        {
          upf_dpi_parse_ip4_packet((ip4_header_t *)pl,
                                   pdr->dpi_path_db_id,
                                   pdr->dpi_host_db_id,
                                   &flow->app_index);
        }
    }
}

always_inline upf_pdr_t *
upf_get_dpi_pdr_by_name (struct rules * active, int direction, u32 app_index)
{
  upf_pdr_t *pdr = NULL;
  upf_pdr_t *res = NULL;
  int iter_direction = 0;

  if (vec_len(active->pdr) == 0)
    return NULL;

  vec_foreach (pdr, active->pdr)
    {
      if (!pdr->app_name)
        continue;

      iter_direction = (pdr->pdi.src_intf == SRC_INTF_ACCESS) ? UL_SDF : DL_SDF;
      if (iter_direction != direction)
        continue;

      if (pdr->app_index == app_index)
        {
          res = pdr;
          break;
        }
    }

  return res;
}

#endif /* __included_upf_dpi_h__ */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
