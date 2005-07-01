/* $Id: forget.c 75 2004-12-03 15:41:48Z lennart $ */

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

#include <time.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "forget.h"
#include "dbstruct.h"
#include "util.h"
#include "dbutil.h"
#include "update.h"

static int copy_entries(struct syrep_db_context *c, struct syrep_db_context *target, uint32_t limit) {
    int ret, r = -1;
    DBT key, data;
    DBC *cursor = NULL;
    unsigned n = 0, t = 0;
    assert(c);

    if ((ret = c->db_id_meta->cursor(c->db_id_meta, NULL, &cursor, 0)) != 0) {
        c->db_id_meta->err(c->db_id_meta, ret, "id_meta");
        goto finish;
    }
    
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
        struct syrep_id *id = (struct syrep_id*) key.data;
        struct syrep_meta *meta = (struct syrep_meta*) data.data;
        struct syrep_name name;
        uint32_t ts;

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            goto finish;
        }

        rotdash();
        t++;

        if ((ts = get_version_timestamp(c, meta->last_seen)) == (uint32_t) -1)
            goto finish;
        
        if (ts < limit)
            continue;

        n++;

        /* copy it */
        if (get_name_by_nrecno(c, &id->nrecno, &name) < 0)
            goto finish;

        if (write_entry(target, &name, &id->md, meta) < 0)
            goto finish;
    }

    if (ret != DB_NOTFOUND) {
        c->db_version_timestamp->err(c->db_id_meta, ret, "id_meta::c_get");
        goto finish;
    }

    if (args.verbose_flag)
        fprintf(stderr, "Removed %u of %u file entries.\n", t-n, t);

    r = 0;

finish:
    if (cursor)
        cursor->c_close(cursor);
    
    return r;
}


static int copy_history(struct syrep_db_context *c, struct syrep_db_context *target, uint32_t limit) {
    int ret, r = -1;
    DBT key, data;
    DBC *cursor = NULL;
    unsigned n = 0, t = 0;
    assert(c);

    if ((ret = c->db_version_timestamp->cursor(c->db_version_timestamp, NULL, &cursor, 0)) != 0) {
        c->db_version_timestamp->err(c->db_version_timestamp, ret, "version_timestamp");
        goto finish;
    }
    
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
        struct syrep_timestamp *timestamp = (struct syrep_timestamp*) data.data;
        assert(timestamp);

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            goto finish;
        }

        rotdash();
        t++;

        if (timestamp->t < limit)
            continue;
        
        n++;

        /* copy it */

        if ((ret = target->db_version_timestamp->put(target->db_version_timestamp, NULL, &key, &data, 0)) != 0) {
            c->db_version_timestamp->err(c->db_version_timestamp, ret, "version_timestamp::put");
            goto finish;
        }
        

    }

    if (ret != DB_NOTFOUND) {
        c->db_version_timestamp->err(c->db_version_timestamp, ret, "version_timestamp::c_get");
        goto finish;
    }

    if (args.verbose_flag)
        fprintf(stderr, "Removed %u of %u history entries.\n", t-n, t);

    r = 0;

finish:
    if (cursor)
        cursor->c_close(cursor);
    
    return r;
}

int forget(struct syrep_db_context *c, struct syrep_db_context *target) {
    time_t t;
    time_t now, limit;

    assert(c && target);

    if (args.remember_arg < 0) {
        fprintf(stderr, "ERROR: The remembrance time has to be greater or equal to 0\n");
        return -1;
    }

    t = (time_t) args.remember_arg*24*60*60;
    time(&now);
    limit = t > now ? 0 : now-t;

    if (args.verbose_flag) {
        fprintf(stderr, "Current time is %s", ctime(&now));
        fprintf(stderr, "Forgetting all entries prior to %s", ctime(&limit));
    }
    
    if (copy_entries(c, target, (uint32_t) limit) < 0)
        return -1;
    
    if (copy_history(c, target, (uint32_t) limit) < 0)
        return -1;

    target->timestamp = c->timestamp;
    target->version = c->version;
    free(target->origin);
    target->origin = strdup(c->origin);
    
    target->modified = 1;

    return 0;
}
