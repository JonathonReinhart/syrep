/* $Id: history.c 43 2003-11-30 14:27:42Z lennart $ */

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <string.h>
#include <time.h>

#include "history.h"
#include "dbstruct.h"

int history(struct syrep_db_context *c) {
    int r = -1, ret;
    DBC *cursor = NULL;
    DBT key, data;

    assert(c && c->db_version_timestamp);

    if ((ret = c->db_version_timestamp->cursor(c->db_version_timestamp, NULL, &cursor, 0)) != 0) {
        c->db_version_timestamp->err(c->db_version_timestamp, ret, "version_timestamp::cursor()");
        goto finish;
    }
    
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
        struct syrep_version *version;
        struct syrep_timestamp *timestamp;

        version = key.data;
        timestamp = data.data;

        assert(version && timestamp);

        fprintf(stderr, "%4u %10u %s", version->v, timestamp->t, ctime((time_t*) (&timestamp->t)));
    }
        
    if (ret != DB_NOTFOUND) {
        c->db_version_timestamp->err(c->db_version_timestamp, ret, "version_timestamp::c_get()");
        goto finish;
    }
    
    r = 0;
    
finish:

    if (cursor)
        cursor->c_close(cursor);

    return r;
}
