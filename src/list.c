/* $Id: list.c 19 2003-08-31 20:46:56Z lennart $ */

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

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "list.h"
#include "context.h"
#include "md5util.h"
#include "dbstruct.h"
#include "util.h"
#include "dbutil.h"
#include "syrep.h"

static int handle_file(struct syrep_db_context *c, const struct syrep_nrecno *nrecno, const struct syrep_md *md, const struct syrep_meta *meta) {
    struct syrep_meta local_meta;
    struct syrep_name name;
    int f;
    
    assert(c && nrecno && md);

    if ((f = get_name_by_nrecno(c, nrecno, &name)) < 0)
        return -1;

    assert(f);
    
    if (!meta) {
        int f;
        if ((f = get_meta_by_nrecno_md(c, nrecno, md, &local_meta)) < 0)
            return -1;

        if (f)
            meta = &local_meta;
    }
        
    if (!args.show_deleted_flag && meta->last_seen != c->version)
        return 0;

    if (meta) {
        if (!args.show_by_md_flag) {
            char d[SYREP_DIGESTLENGTH*2+1]; 
            fhex(md->digest, SYREP_DIGESTLENGTH, d);
            d[SYREP_DIGESTLENGTH*2] = 0;
            
            printf("%s %s%s", d, name.path, meta->last_seen == c->version ? "\t\t" : "\t(deleted)");
        } else
            printf("\t%s%s", name.path, meta->last_seen == c->version ? "\t\t" : "\t(deleted)");
        
        if (args.show_times_flag)
            printf( "\t(first-seen: %u; last-seen: %u)\n", meta->first_seen, meta->last_seen);
        else
            fputc('\n', stdout);
    } else {
        char d[SYREP_DIGESTLENGTH*2+1]; 
        fhex(md->digest, SYREP_DIGESTLENGTH, d);
        d[SYREP_DIGESTLENGTH*2] = 0;
        
        printf("\t%s", name.path);
    }
            
    return 0;
}

int list(struct syrep_db_context *c) {
    int r = -1, ret;
    DBC *cursor = NULL;
    DBT key, data;

    if (args.show_by_md_flag) {
        struct syrep_md previous_md;
        memset(&previous_md, 0, sizeof(previous_md));
        
        if ((ret = c->db_md_nrecno->cursor(c->db_md_nrecno, NULL, &cursor, 0)) != 0) {
            c->db_md_nrecno->err(c->db_md_nrecno, ret, "md_nrecno");
            goto finish;
        }
        
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));

        while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
            struct syrep_md *md = (struct syrep_md*) key.data;
            struct syrep_nrecno *recno = (struct syrep_nrecno*) data.data;
            struct syrep_meta meta;

            if (memcmp(&previous_md, md, sizeof(previous_md))) {
                char d[SYREP_DIGESTLENGTH*2+1]; 
                fhex(md->digest, SYREP_DIGESTLENGTH, d);
                d[SYREP_DIGESTLENGTH*2] = 0;
                printf("%s:\n", d);
                memcpy(&previous_md, md, sizeof(previous_md));
            }
                
            if ((ret = get_meta_by_nrecno_md(c, recno, md, &meta)) < 0)
                goto finish;

            if (handle_file(c, recno, md, &meta) < 0)
                fprintf(stderr, "handle_file() failed\n");
        }
        
        if (ret != DB_NOTFOUND) {
            c->db_md_nrecno->err(c->db_md_nrecno, ret, "md_nrecno::c_get");
            goto finish;
        }
    
        r = 0;
    } else {

        if ((ret = c->db_id_meta->cursor(c->db_id_meta, NULL, &cursor, 0)) != 0) {
            c->db_id_meta->err(c->db_id_meta, ret, "id_meta");
            goto finish;
        }
        
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));

        while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
            struct syrep_id *id = (struct syrep_id*) key.data;
            
            if (handle_file(c, &id->nrecno, &id->md, (struct syrep_meta*) data.data) < 0)
                fprintf(stderr, "handle_file() failed\n");
            
        }
        
        if (ret != DB_NOTFOUND) {
            c->db_id_meta->err(c->db_id_meta, ret, "id_meta::c_get");
            goto finish;
        }
    
        r = 0;
    }
    
finish:

    if (cursor)
        cursor->c_close(cursor);

    return r;
}
