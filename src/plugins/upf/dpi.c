/*
 * dpi.c - 3GPP TS 29.244 UPF DPI
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

#define _LGPL_SOURCE            /* LGPL v3.0 is compatible with Apache 2.0 */

#include <vppinfra/types.h>
#include <vppinfra/vec.h>

#include <hs.h>
#include "upf/dpi.h"

static hs_database_t *database = NULL;
static hs_compile_error_t *compile_err = NULL;
static hs_scratch_t *scratch = NULL;

int
upf_dpi_add_multi_regex(upf_dpi_args_t * args, u32 * db_index)
{
  *db_index = 0;

  if (hs_compile_multi(args->rules, args->flags,
                       args->indecies, vec_len(args->rules),
                       HS_MODE_BLOCK, NULL, &database,
                       &compile_err) != HS_SUCCESS)
    {
      return -1;
    }

  if (hs_alloc_scratch(database, &scratch) != HS_SUCCESS)
    {
      hs_free_database(database);
      database = NULL;
      return -1;
    }

  return 0;
}

static int
upf_dpi_event_handler(unsigned int id, unsigned long long from,
                                 unsigned long long to, unsigned int flags,
                                void *ctx)
{
  (void) from;
  (void) to;
  (void) flags;

  u32 *app_id = (u32*)ctx;

  *app_id = id;

  return 0;
}

int
upf_dpi_lookup(u32 db_index, const char * str, uint16_t length, u32 * app_index)
{
  (void) db_index;
  int ret = 0;

  ret = hs_scan(database, str, length, 0, scratch, upf_dpi_event_handler, (void*)app_index);
    if (ret != HS_SUCCESS)
    {
      return -1;
    }

  return 0;
}

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
