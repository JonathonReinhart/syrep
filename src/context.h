#ifndef foocontexthfoo
#define foocontexthfoo

/* $Id: context.h 30 2003-09-04 22:06:56Z lennart $ */

/***
  This file is part of syrep.

  syrep is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  syrep is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.
  
  You should have received a copy of the GNU General Public License
  along with syrep; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***/

#include <db.h>
#include <inttypes.h>

struct syrep_db_context {
    struct package *package;

    DB  *db_id_meta,         
        *db_md_nrecno,
        *db_nrecno_md,
        *db_nrecno_lastmd,
        *db_md_lastnrecno,
        *db_version_timestamp,
        *db_nhash_nrecno,
        *db_nrecno_name;

    uint32_t timestamp;
    uint32_t version;
    int modified;

    char* origin;
};

struct syrep_db_context* db_context_open(const char *path, int force);
int db_context_save(struct syrep_db_context *c, const char *path);
int db_context_free(struct syrep_db_context* c);

int db_context_origin_warn(struct syrep_db_context *c);

#endif
