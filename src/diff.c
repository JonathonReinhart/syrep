/* $Id: diff.c 57 2004-07-18 18:47:55Z lennart $ */

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
#include <stdlib.h>

#include "diff.h"
#include "dbstruct.h"
#include "util.h"
#include "md5util.h"
#include "dbutil.h"

static int add_diff_entry(DB *ddb, struct syrep_name *name, int action, struct syrep_db_context *repository) {
    DBT key, data;
    int ret;
    struct diff_entry de;

    memset(&de, 0, sizeof(de));
    de.action = action;
    de.repository = repository;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = name;
    key.size = sizeof(struct syrep_name);

    data.data = &de;
    data.size = sizeof(struct diff_entry);

    if ((ret = ddb->put(ddb, NULL, &key, &data, DB_NOOVERWRITE)) != 0) {
        DBT data2;

        if (ret != DB_KEYEXIST) {
            ddb->err(ddb, ret, "ddb::put()");
            return -1;
        }
        
        memset(&data2, 0, sizeof(data2));
        
        if ((ret = ddb->get(ddb, NULL, &key, &data2, 0)) != 0) {
            ddb->err(ddb, ret, "ddb::get()");
            return -1;
        }
        
        if (data.size != data2.size || memcmp(data.data, data2.data, data.size)) {
            fprintf(stderr, "Snapshot inconsistency %u %u\n", data.size, data2.size);
            return -1;
        }
    }
    
    return 0;
}

static int foreach(DB *ddb, struct syrep_db_context *c1, struct syrep_db_context *c2, struct syrep_name *name) {
    struct syrep_md md1, md2;
    struct syrep_nrecno nrecno1, nrecno2;
    int md1_valid = 0, md2_valid = 0;
    int nrecno1_valid, nrecno2_valid;

    if ((nrecno1_valid = get_nrecno_by_name(c1, name, &nrecno1, 0)) < 0)
        return -1;

    if (nrecno1_valid)
        if ((md1_valid = get_current_md_by_nrecno(c1, &nrecno1, &md1)) < 0)
            return -1;

    if ((nrecno2_valid = get_nrecno_by_name(c2, name, &nrecno2, 0)) < 0)
        return -1;

    if (nrecno2_valid)
        if ((md2_valid = get_current_md_by_nrecno(c2, &nrecno2, &md2)) < 0)
            return -1;

    //fprintf(stderr, "FOREACH %i %i %s\n", md1_valid, md2_valid, name->path);
        
    if (md1_valid && md2_valid) {
        int f1 = 0, f2 = 0;

        /* Same file? */
        if (!memcmp(&md1, &md2, sizeof(struct syrep_md)))
            return 0;

        if (nrecno1_valid)
            if ((f1 = get_meta_by_nrecno_md(c1, &nrecno1, &md2, NULL)) < 0)
                return -1;

        if (nrecno2_valid)
            if ((f2 = get_meta_by_nrecno_md(c2, &nrecno2, &md1, NULL)) < 0)
                return -1;

        /* The version in c1 is a newer version of that in c2 */
        if (f1 && !f2)
            return add_diff_entry(ddb, name, DIFF_COPY, c1);

        /* Vice versa */
        if (!f1 && f2)
            return add_diff_entry(ddb, name, DIFF_COPY, c2);

        /* Completely different file */
        return add_diff_entry(ddb, name, DIFF_CONFLICT, NULL);

    } else if (md1_valid) {
        struct syrep_meta meta1, meta2;
        int f1, f2;
        uint32_t t1, t2;

        if (nrecno2_valid)
            if ((md2_valid = get_last_md_by_nrecno(c2, &nrecno2, &md2)) < 0)
                return -1;

        if (!md2_valid)
            return add_diff_entry(ddb, name, DIFF_COPY, c1);

        if (memcmp(&md1, &md2, sizeof(struct syrep_md)))
            return add_diff_entry(ddb, name, DIFF_COPY, c1);

        if ((f1 = get_meta_by_nrecno_md(c1, &nrecno1, &md1, &meta1)) < 0)
            return -1;

        if ((f2 = get_meta_by_nrecno_md(c2, &nrecno2, &md2, &meta2)) < 0)
            return -1;
        
        if (!f1 || !f2) {
            fprintf(stderr, "Database inconsistency\n");
            return -1;
        }

        /* Check whether file reappeared in c1 */
        if ((t1 = get_version_timestamp(c1, meta1.first_seen-1)) == (uint32_t) -1)
            return -1;

        if ((t2 = get_version_timestamp(c2, meta2.last_seen+1)) == (uint32_t) -1)
            return -1;

        if (t1 >= t2)
            return add_diff_entry(ddb, name, DIFF_COPY, c1);

        
        /* Check whether file was deleted in c2 */
        if ((t1 = get_version_timestamp(c1, meta1.first_seen)) == (uint32_t) -1)
            return -1;

        if ((t2 = get_version_timestamp(c2, meta2.last_seen)) == (uint32_t) -1)
            return -1;

        if (t1 < t2)
            return add_diff_entry(ddb, name, DIFF_DELETE, c1);

        return add_diff_entry(ddb, name, DIFF_CONFLICT, NULL);

    } else if (md2_valid) {
        fprintf(stderr, "This should be impossible!\n");
        abort();
    }

    return 0;
}


static int enumerate(DB *ddb, struct syrep_db_context *c1, struct syrep_db_context *c2) {
    int r = -1, ret;
    DBC *cursor = NULL;
    DBT key, data;

    if ((ret = c1->db_id_meta->cursor(c1->db_id_meta, NULL, &cursor, 0)) != 0) {
        c1->db_id_meta->err(c1->db_id_meta, ret, "id_meta::cursor()");
        goto finish;
    }

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    
    while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
        struct syrep_id *id = (struct syrep_id*) key.data;
        struct syrep_meta *meta = (struct syrep_meta*) data.data;
        struct syrep_name name;
        int f;

        assert(id && meta);

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            goto finish;
        }
        
        if (meta->last_seen != c1->version)
            continue;

        if ((f = get_name_by_nrecno(c1, &id->nrecno, &name)) < 0)
            return -1;

        if (f) {
            if (foreach(ddb, c1, c2, &name) < 0) {
                fprintf(stderr, "foreach() failed\n");
                goto finish;
            }
        }

        rotdash();
    }


    if (ret != DB_NOTFOUND) {
        c1->db_id_meta->err(c1->db_id_meta, ret, "id_meta::c_get()");
        goto finish;
    }

    r = 0;

finish:

    if (cursor)
        cursor->c_close(cursor);

    return r;
}

DB* make_diff(struct syrep_db_context *c1, struct syrep_db_context *c2) {
    int ret;
    DB *ddb = NULL;

    if ((ret = db_create(&ddb, NULL, 0))) {
        fprintf(stderr, "ddb::create(): %s\n", db_strerror(ret));
        goto finish;
    }

    if ((ret = ddb->open(ddb, NULL, NULL, NULL, DB_BTREE, DB_CREATE, 0664))) {
        ddb->err(ddb, ret, "ddb::open()");
        goto finish;
    }
    
    if (enumerate(ddb, c1, c2) < 0)
        goto finish;

    if (enumerate(ddb, c2, c1) < 0)
        goto finish;

    rotdash_hide();
    
    return ddb;
    
finish:

    if (ddb)
        ddb->close(ddb, 0);
    
    return NULL;
}

struct cb_info {
    struct syrep_db_context *c1, *c2;
    const char *p1, *p2;
    off_t sum1, sum2;
};

static int list_cb(DB *ddb, struct syrep_name *name, struct diff_entry *de, void *p) {
    struct syrep_md md1, md2;
    struct syrep_nrecno nrecno1, nrecno2;
    int f1, f2;
    struct cb_info *cb_info = p;
    
    assert(name && de);

    if ((f1 = get_nrecno_by_name(cb_info->c1, name, &nrecno1, 0)) < 0)
        return -1;
    
    if (f1)
        if ((f1 = get_last_md_by_nrecno(cb_info->c1, &nrecno1, &md1)) < 0)
            return -1;

    if ((f2 = get_nrecno_by_name(cb_info->c2, name, &nrecno2, 0)) < 0)
        return -1;

    if (f2)
        if ((f2 = get_last_md_by_nrecno(cb_info->c2, &nrecno2, &md2)) < 0)
            return -1;
    
    if (!(f1 || f2)) {
        fprintf(stderr, "Diff inconsicteny\n");
        return -1;
    }
    
    switch (de->action) {
        case DIFF_COPY: {
            char d[33];
            char sizet[100] = " ", *psizet = "";
            char dst;
            const char *root = NULL;
            int mf;
            
            if ((mf = get_current_nrecno_by_md(de->repository == cb_info->c1 ? cb_info->c2 : cb_info->c1,
                                               de->repository == cb_info->c1 ? &md1 : &md2, NULL)) < 0)
                return -1;
            
            if (de->repository == cb_info->c1) {
                root = cb_info->p1;
                dst = 'B';
                fhex_md5(md1.digest, d);
            } else {
                root = cb_info->p2;
                dst = 'A';
                fhex_md5(md2.digest, d);
            }
            
            d[32] = 0;

            if (args.sizes_flag && root && !mf) {
                off_t fsize;
                char cpath[PATH_MAX];
                snprintf(cpath, sizeof(cpath), "%s/%s", root, name->path);

                if ((fsize = filesize2(cpath)) != (off_t) -1) {
                    strcpy(sizet, " (");
                    snprint_off(sizet+2, sizeof(sizet)-3, fsize);
                    strcat(sizet, ")");
                    psizet = sizet;


                    if (de->repository == cb_info->c1)
                        cb_info->sum1 += fsize;
                    else
                        cb_info->sum2 += fsize;
                }
            }

            printf("COPY <%s|%s> TO %c%s%s\n", d, name->path, dst, mf ? " (LINK POSSIBLE)" : "", psizet);
            
            break;
        }
            
        case DIFF_DELETE: {
            char d[33];
            int rep;
            
            if (de->repository == cb_info->c1) {
                rep = 'A';
                fhex_md5(md1.digest, d);
            } else {
                rep = 'B';
                fhex_md5(md2.digest, d);
            }
            
            d[32] = 0;
            
            printf("DELETE <%s|%s> FROM %c\n", d, name->path, rep);
            break;
        }
            
        case DIFF_CONFLICT: {
            char d1[33], d2[33];
            
            fhex_md5(md1.digest, d1);
            fhex_md5(md2.digest, d2);
            
            d1[32] = d2[32] = 0;
            
            printf("CONFLICT <%s> BETWEEN <%s> IN A AND <%s> IN B\n", name->path, d1, d2);
            break;
        }
    }

    return 0;
}

int list_diff(struct syrep_db_context *c1, struct syrep_db_context *c2, DB *ddb, const char *p1, const char *p2) {
    struct cb_info cb_info;
    char sumt[100];
    cb_info.c1 = c1;
    cb_info.c2 = c2;
    cb_info.p1 = p1;
    cb_info.p2 = p2;
    cb_info.sum1 = cb_info.sum2 = 0;
    
    if (diff_foreach(ddb, list_cb, &cb_info) < 0)
        return -1;

    if (cb_info.sum1)
        printf("TOTAL SUM FROM A: %s\n", snprint_off(sumt, sizeof(sumt), cb_info.sum1));
    if (cb_info.sum2)
        printf("TOTAL SUM FROM B: %s\n", snprint_off(sumt, sizeof(sumt), cb_info.sum2));
    return 0;
}

int diff_foreach(DB *ddb, int (*cb)(DB *ddb, struct syrep_name *name, struct diff_entry *de, void *p), void *p) {
    DBC *cursor = NULL;
    int r = -1, ret;
    DBT key, data;

    if ((ret = ddb->cursor(ddb, NULL, &cursor, 0)) != 0) {
        ddb->err(ddb, ret, "ddb::cursor()");
        goto finish;
    }

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    
    while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
        int t;
        struct syrep_name *name = (struct syrep_name*) key.data;
        struct diff_entry *de = (struct diff_entry*) data.data;

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            goto finish;
        }
        
        if ((t = cb(ddb, name, de, p)) < 0) {
            r = t;
            goto finish;
        }
    }
    
    if (ret != DB_NOTFOUND) {
        ddb->err(ddb, ret, "ddb::c_get() failed");
        goto finish;
    }

    r = 0;
    
finish:
    if (cursor)
        cursor->c_close(cursor);

    return r;
}
