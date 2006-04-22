/* $Id: dbutil.c 103 2006-04-22 10:57:59Z lennart $ */

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

#include <string.h>
#include <assert.h>
#include <time.h>

#include <zlib.h>

#include "dbutil.h"
#include "util.h"

int get_meta_by_nrecno_md(struct syrep_db_context *c, const struct syrep_nrecno*nrecno, const struct syrep_md *md, struct syrep_meta *meta) {
    int ret;
    struct syrep_id id;
    DBT key, data;

    assert(c && c->db_id_meta && nrecno);

    memset(&id, 0, sizeof(id));
    memcpy(&id.nrecno, nrecno, sizeof(struct syrep_nrecno));
    memcpy(&id.md, md, sizeof(struct syrep_md));
    
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = &id;
    key.size = sizeof(struct syrep_id);
    
    if ((ret = c->db_id_meta->get(c->db_id_meta, NULL, &key, &data, 0))) {
        if (ret == DB_NOTFOUND)
            return 0;
        
        c->db_id_meta->err(c->db_id_meta, ret, "id_meta::get");
        return -1;
    }

    assert(data.data);
    
    if (meta)
        memcpy(meta, data.data, sizeof(struct syrep_meta));
    
    return 1;
}


int get_current_nrecno_by_md(struct syrep_db_context *c, const struct syrep_md *md, struct syrep_nrecno *nrecno) {
    int ret, f;
    struct syrep_meta meta;
    DBT key, data;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = (void*) md;
    key.size = sizeof(struct syrep_md);

    if ((ret = c->db_md_lastnrecno->get(c->db_md_lastnrecno, NULL, &key, &data, 0))) {
        if (ret == DB_NOTFOUND)
            return 0;
        
        c->db_md_lastnrecno->err(c->db_md_lastnrecno, ret, "md_lastnrecno::get()");
        return -1;
    }

    assert(data.data);

    if ((f = get_meta_by_nrecno_md(c, (struct syrep_nrecno*) data.data, md, &meta)) < 0)
        return -1;

    if (!f) {
        fprintf(stderr, "Database inconsistency\n");
        return -1;
    }
        
    if (meta.last_seen != c->version)
        return 0;
    
    if (nrecno)
        memcpy(nrecno, data.data, sizeof(struct syrep_nrecno));
    
    return 1;
}

int get_last_md_by_nrecno(struct syrep_db_context *c, const struct syrep_nrecno *nrecno, struct syrep_md *md) {
    int ret;
    DBT key, data;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = (void*) nrecno;
    key.size = sizeof(struct syrep_nrecno);

    if ((ret = c->db_nrecno_lastmd->get(c->db_nrecno_lastmd, NULL, &key, &data, 0))) {
        if (ret == DB_NOTFOUND) {
            return 0;
        }
        
        c->db_nrecno_lastmd->err(c->db_nrecno_lastmd, ret, "nrecno_lastmd::get()");
        return -1;
    }


    assert(data.data);
    if (md)
        memcpy(md, data.data, sizeof(struct syrep_md));

    return 1;
}


int get_current_md_by_nrecno(struct syrep_db_context *c, const struct syrep_nrecno *nrecno, struct syrep_md *md) {
    struct syrep_md lmd;
    struct syrep_meta meta;
    int f;

    if ((f = get_last_md_by_nrecno(c, nrecno, &lmd)) < 0)
        return -1;
    
    if (!f)
        return 0;

    if ((f = get_meta_by_nrecno_md(c, nrecno, &lmd, &meta)) < 0)
        return -1;

    if (!f) {
        fprintf(stderr, "Database inconsistency\n");
        return -1;
    }

    if (meta.last_seen != c->version)
        return 0;

    memcpy(md, &lmd, sizeof(struct syrep_md));
    return 1;
}

int get_current_md_by_name(struct syrep_db_context *c, const struct syrep_name *name, struct syrep_md *md) {
    struct syrep_nrecno nrecno;

    if (get_nrecno_by_name(c, name, &nrecno, 0) < 0)
        return -1;

    return get_current_md_by_nrecno(c, &nrecno, md);
}

uint32_t get_version_timestamp(struct syrep_db_context *c, uint32_t v) {
    DBT key, data;
    struct syrep_version version;
    struct syrep_timestamp *timestamp;
    int ret;
    
    assert(c && c->db_version_timestamp);

    if (v > c->version)
        return time(NULL);

    if (v <= 0)
        v = 1;

    memset(&version, 0, sizeof(version));
    version.v = v;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = &version;
    key.size = sizeof(version);

    if ((ret = c->db_version_timestamp->get(c->db_version_timestamp, NULL, &key, &data, 0))) {

        /* If the specific history entry was lost: return the epoch */
        if (ret == DB_NOTFOUND)
            return 0;
        
        c->db_version_timestamp->err(c->db_version_timestamp, ret, "version_timestamp::get");
        return (uint32_t) -1;
    }

    timestamp = (struct syrep_timestamp*) data.data;
    assert(timestamp);

    return timestamp->t;
}

int get_name_by_nrecno(struct syrep_db_context *c, const struct syrep_nrecno *nrecno, struct syrep_name *name) {
    DBT key, data;
    int ret;
    
    assert(c && nrecno);

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = (void*) &nrecno->recno;
    key.size = sizeof(nrecno->recno);

    if ((ret = c->db_nrecno_name->get(c->db_nrecno_name, NULL, &key, &data, 0))) {
        if (ret == DB_NOTFOUND) {
            return 0;
        }
        
        c->db_nrecno_name->err(c->db_nrecno_name, ret, "nrecno_name::get");
        return -1;
    }

    if (name) {
        memset(name, 0, sizeof(struct syrep_name));
        strncpy(name->path, data.data, MIN(PATH_MAX-1, data.size));
        name->path[PATH_MAX-1] = 0;
    }

    return 1;
}

static uint32_t csum_name(const struct syrep_name *name) {
    uint32_t a = 0;

    assert(name);
    
    a = adler32(0, NULL, 0); 
    a = adler32(a, (const uint8_t*) name->path, strlen(name->path)); 
    
    /*fprintf(stderr, "csum: %s -> %u\n", name->path, a);*/
    
    return a;
}

int get_nrecno_by_name(struct syrep_db_context *c, const struct syrep_name *rname, struct syrep_nrecno *rnrecno, int create) {
    struct syrep_nhash nhash;
    DBT key, data;
    DBC *cursor = NULL;
    int r = -1, ret;

    assert(c && rname);
    
    if ((ret = c->db_nhash_nrecno->cursor(c->db_nhash_nrecno, NULL, &cursor, 0)) != 0) {
        c->db_nhash_nrecno->err(c->db_nhash_nrecno, ret, "nhash_nrecno");
        goto finish;
    }

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    nhash.hash = csum_name(rname);
    key.data = &nhash;
    key.size = sizeof(nhash);

    ret = cursor->c_get(cursor, &key, &data, DB_SET);

    for(;;) {
        struct syrep_nrecno nrecno;
        struct syrep_name name;

        int f;

        if (ret) {
            if (ret == DB_NOTFOUND) {

                r = 0;
                goto finish;
            }

            c->db_nhash_nrecno->err(c->db_nhash_nrecno, ret, "nhash_nrecno::get");
            goto finish;
        }


        if (memcmp(key.data, &nhash, sizeof(struct syrep_nhash))) {
            r = 0;
            goto finish;
        }
            
        memcpy(&nrecno, data.data, sizeof(nrecno));

        if ((f = get_name_by_nrecno(c,  &nrecno, &name)) < 0)
            goto finish;

        if (f && !strcmp(name.path, rname->path)) {
            if (rnrecno)
                memcpy(rnrecno, &nrecno, sizeof(struct syrep_nrecno));

            r = 1;
            goto finish;
        }
        
        ret = cursor->c_get(cursor, &key, &data, DB_NEXT);
    }

finish:

    if (cursor)
        cursor->c_close(cursor);

    if (!r && create) {
        r = new_name(c, rname, rnrecno) < 0 ? -1 : 1;
    }
    
    return r;
}

int new_name(struct syrep_db_context *c, const struct syrep_name *name, struct syrep_nrecno *rnrecno) {
    struct syrep_nhash nhash;
    struct syrep_nrecno nrecno;
    int ret;
    DBT key, data;
    assert(c && name);

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    data.data = (void*) name->path;
    data.size = strlen(name->path);
    
    if ((ret = c->db_nrecno_name->put(c->db_nrecno_name, NULL, &key, &data, DB_APPEND))) {
        c->db_nrecno_name->err(c->db_nrecno_name, ret, "nrecno_name::put");
        return -1;
    }

    nrecno.recno = *((db_recno_t*) key.data);

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    nhash.hash = csum_name(name);
    key.data = &nhash;
    key.size = sizeof(nhash);

    data.data = &nrecno;
    data.size = sizeof(nrecno);

    if ((ret = c->db_nhash_nrecno->put(c->db_nhash_nrecno, NULL, &key, &data, DB_NODUPDATA))) {
        c->db_nhash_nrecno->err(c->db_nhash_nrecno, ret, "nhash_nrecno::put");
        return -1;
    }
    
    if (rnrecno)
        memcpy(rnrecno, &nrecno, sizeof(struct syrep_nrecno));
    
    return 0;
}
