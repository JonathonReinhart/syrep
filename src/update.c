/* $Id: update.c 19 2003-08-31 20:46:56Z lennart $ */

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

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include "update.h"
#include "dbstruct.h"
#include "util.h"
#include "dbutil.h"
#include "md5util.h"

static int dbput(DB* db, const void *k, int klen, const void*d, int dlen, int f) {
    DBT key, data;
    int ret;

    memset(&key, 0, sizeof(key));
    key.data = (void*) k;
    key.size = klen;

    memset(&data, 0, sizeof(data));
    data.data = (void*) d;
    data.size = dlen;
        
    if ((ret = db->put(db, NULL, &key, &data, f)) != 0) {
        if (ret != DB_KEYEXIST) {
            db->err(db, ret, "db::put()");
            return -1;
        }

        return 0;
    }

    return 1;
}

static int write_entry(struct syrep_db_context *c, const struct syrep_name *name, const struct syrep_md *md, const struct syrep_meta *meta) {
    struct syrep_id id;
    struct syrep_nrecno nrecno;
    int f;

    assert(c && c->db_id_meta && c->db_md_nrecno && c->db_nrecno_md && c->db_md_lastnrecno && c->db_nrecno_lastmd && name && md && meta);

    if ((f = get_nrecno_by_name(c, name, &nrecno, 1)) < 0)
        return -1;

    assert(f);
    
    /*** Update id_meta ***/
    memset(&id, 0, sizeof(id));
    memcpy(&id.nrecno, &nrecno, sizeof(struct syrep_nrecno));
    memcpy(&id.md, md, sizeof(struct syrep_md));

    if (dbput(c->db_id_meta, &id, sizeof(struct syrep_id), meta, sizeof(struct syrep_meta), 0) < 0)
        return -1;

    /*** Update md_nrecno ***/
    if (dbput(c->db_md_nrecno, md, sizeof(struct syrep_md), &nrecno, sizeof(struct syrep_nrecno), DB_NODUPDATA) < 0)
        return -1;

    /*** Update nrecno_md ***/
    if (dbput(c->db_nrecno_md, &nrecno, sizeof(struct syrep_nrecno), md, sizeof(struct syrep_md), DB_NODUPDATA) < 0)
        return -1;

    /*** Update md_lastnrecno ***/
    if (dbput(c->db_md_lastnrecno, md, sizeof(struct syrep_md), &nrecno, sizeof(struct syrep_nrecno), 0) < 0)
        return -1;

    /*** Update nrecno_lastmd ***/
    if ((f = dbput(c->db_nrecno_lastmd, &nrecno, sizeof(struct syrep_nrecno), md, sizeof(struct syrep_md), 0)) < 0)
        return -1;

    //fprintf(stderr, "Insert: %s %i\n", name->path, f);
    
    c->modified = 1;
    return 0;
}

static int handle_file(struct syrep_db_context *c, uint32_t version, const char *path, const struct syrep_md *md) {
    int r;
    struct syrep_meta meta;
    struct syrep_name name;
    struct syrep_nrecno nrecno;

    memset(&name, 0, sizeof(name));
    strncpy(name.path, path, PATH_MAX);

    if ((r = get_nrecno_by_name(c, &name, &nrecno, 0)) < 0)
        return -1;

    if (r) {
    
        memset(&meta, 0, sizeof(meta));
        
        if ((r = get_meta_by_nrecno_md(c, &nrecno, md, &meta)) < 0)
            return -1;
    }

    if (r) { /* File is alread known */

        if (meta.last_seen != c->version) { /* File was deleted preiously */
            if (args.verbose_flag)
                fprintf(stderr, "%s: File reappeared\n", path);
            meta.first_seen = meta.last_seen = version;
        } else  { /* File is not new */
            if (args.verbose_flag)
                fprintf(stderr, "%s: File still available\n", path);            
            meta.last_seen = version;
        }

    } else { /* File is new */
        if (args.verbose_flag)
            fprintf(stderr, "%s: File is new\n", path);
        meta.first_seen = meta.last_seen = version;
    }
        
    return write_entry(c, &name, md, &meta);
}

static int iterate_dir(struct syrep_db_context *c, struct syrep_md_cache *cache, uint32_t version, const char *root) {
    int r = -1;
    DIR *dir;
    struct dirent *de;
    char p[PATH_MAX];

    if (!(dir = opendir(root)))
        return -1;

    while ((de = readdir(dir))) {
        struct syrep_md md;
        struct stat st;
        
        if (de->d_name[0] == '.')
            continue;

        if (!strncmp(de->d_name, ".syrep", 6))
            continue;

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            goto finish;
        }
        
        rotdash();
        
        snprintf(p, sizeof(p), "%s/%s", root, de->d_name);

        normalize_path(p);

        if (stat(p, &st) < 0) {
            fprintf(stderr, "stat(%s) failed: %s\n", p, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (iterate_dir(c, cache, version, p) < 0)
                goto finish;
            
        } else if (S_ISREG(st.st_mode)) {
            if (md_cache_get(cache, p, md.digest) < 0)
                goto finish;
            
            if ((handle_file(c, version, p, &md)) < 0)
                goto finish;
        }
    }

    r = 0;
    
finish:
        
    closedir(dir);

    return r;
}

static int new_version(struct syrep_db_context *c, uint32_t v, uint32_t t) {
    DBT key, data;
    struct syrep_version version;
    struct syrep_timestamp timestamp;
    int ret;

    assert(c && c->db_version_timestamp);
    
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    memset(&version, 0, sizeof(version));
    memset(&timestamp, 0, sizeof(timestamp));

    version.v = v;
    key.data = &version;
    key.size = sizeof(version);
    
    timestamp.t = t;
    data.data = &timestamp;
    data.size = sizeof(timestamp);

    if ((ret = c->db_version_timestamp->put(c->db_version_timestamp, NULL, &key, &data, 0))) {
        c->db_version_timestamp->err(c->db_version_timestamp, ret, "version_timestamp::get");
        return -1;
    }

    c->timestamp = t;
    c->version = v;
    
    return 0;
}

int update(struct syrep_db_context *c, struct syrep_md_cache *cache) {
    uint32_t now, version;

    assert(c);
    
    now = (uint32_t) time(NULL);
    version = c->version+1;

    if (iterate_dir(c, cache, version, ".") < 0)
        return -1;

    rotdash_hide();

    if (new_version(c, version, now) < 0)
        return -1;
    
    if (args.verbose_flag)
        fprintf(stderr, "Wrote version %u.\n", c->version);

    return 0;
}
