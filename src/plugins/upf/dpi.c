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

#include <hs.h>
#include "upf/dpi.h"

#define UPF_DPI_REGEX_NUM_MAX 100
#define CFG_VALUE_LEN 64

static hs_database_t *database = NULL;
static hs_compile_error_t *compile_err = NULL;
static hs_scratch_t *scratch = NULL;

static const char *patterns[UPF_DPI_REGEX_NUM_MAX] = {};
static unsigned patterns_ids[UPF_DPI_REGEX_NUM_MAX] = {};
static unsigned flags[UPF_DPI_REGEX_NUM_MAX] = {};
static u8* names[UPF_DPI_REGEX_NUM_MAX] = {};

int
upf_dpi_add_multi_regex(u8 * app_name, u8 * regex_array, int regex_num)
{
  int i = 0;
  u8 length = 0;

  if (regex_num > UPF_DPI_REGEX_NUM_MAX)
    return -1;

  if (database)
    {
      hs_free_database(database);
      database = NULL;
    }

  for (i = 0; i < regex_num; i++)
    {
      length = MIN(CFG_VALUE_LEN - 1, strlen(regex_array[i]));
      patterns[i] = entries[i].value;
      patterns_ids[i] = i;
      flags[i] = HS_FLAG_DOTALL;
    }

  if (hs_compile_multi(patterns, flags, patterns_ids, regex_num,
                       HS_MODE_BLOCK, NULL, &database, &compile_err) != HS_SUCCESS)
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

  char **name = (char**)ctx;

  *name = names[id];

  return 0;
}

int
upf_dpi_lookup(u8 * str, uint16_t length, char **app_name)
{
  int ret = 0;

  ret = hs_scan(database, str, length, 0, scratch, upf_dpi_event_handler, (void*)app_name);
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
