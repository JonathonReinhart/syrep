/* $Id: context.c 43 2003-11-30 14:27:42Z lennart $ */

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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "context.h"
#include "package.h"
#include "util.h"

int db_context_free(struct syrep_db_context* c) {
    if (c) {

        if (c->db_id_meta)
            c->db_id_meta->close(c->db_id_meta, 0);
        
        if (c->db_md_nrecno)
            c->db_md_nrecno->close(c->db_md_nrecno, 0);

        if (c->db_nrecno_md)
            c->db_nrecno_md->close(c->db_nrecno_md, 0);

        if (c->db_md_lastnrecno)
            c->db_md_lastnrecno->close(c->db_md_lastnrecno, 0);

        if (c->db_nrecno_lastmd)
            c->db_nrecno_lastmd->close(c->db_nrecno_lastmd, 0);
        
        if (c->db_version_timestamp)
            c->db_version_timestamp->close(c->db_version_timestamp, 0);

        if (c->db_nhash_nrecno)
            c->db_nhash_nrecno->close(c->db_nhash_nrecno, 0);

        if (c->db_nrecno_name)
            c->db_nrecno_name->close(c->db_nrecno_name, 0);
        
        if (c->package)
            package_remove(c->package);

        if (c->origin)
            free(c->origin);
        
        free(c);
    }

    return 0;
}


static DB* open_db(const char*path, int dup, int recno) {
    int ret;
    DB* db;

    if ((ret = db_create(&db, NULL, 0))) {
        fprintf(stderr, "db_create: %s\n", db_strerror(ret));
        return NULL;
    }

    if (dup && !recno)
        db->set_flags(db, DB_DUPSORT);

    //db->set_pagesize(db, 4096*8);

    if ((ret = db->open(db, NULL, path, NULL, recno ? DB_RECNO : DB_BTREE, DB_CREATE, 0664))) {
        db->err(db, ret, "open(%s)", path);
        db->close(db, 0);
        return NULL;
    }

    return db;
}

struct syrep_db_context* db_context_open(const char *filename, int force) {
    struct syrep_db_context *c = NULL;
    const char* path;
    FILE *f;
    int k;

    if (!(c = malloc(sizeof(struct syrep_db_context))))
        goto fail;

    memset(c, 0, sizeof(struct syrep_db_context));

    if (!(c->package = package_open(filename, force)))
        goto fail;

    if ((k = package_get_item(c->package, "timestamp", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if ((f = fopen(path, "r"))) {
        if (fscanf(f, "%i", &c->timestamp) != 1)
            c->timestamp = 0;
        fclose(f);
    }

    if (!c->timestamp)
        c->timestamp = time(NULL);

    if ((k = package_get_item(c->package, "version", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if ((f = fopen(path, "r"))) {
        if (fscanf(f, "%u", &c->version) != 1)
            c->version = 0;
        fclose(f);
    }
    
    if ((k = package_get_item(c->package, "origin", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if ((f = fopen(path, "r"))) {
        char hn[256];
        if (fgets(hn, sizeof(hn), f)) {
            char *nl;
            
            if ((nl = strchr(hn, '\n')))
                *nl = 0;
                
            if (hn[0])
                c->origin = strdup(hn);
        }
        
        fclose(f);
    } else
        c->version = 0;

    if (!c->origin) {
        char hn[256];
        if (gethostname(hn, sizeof(hn)) < 0)
            goto fail;

        c->origin = strdup(hn);
    }

    /* Creating database id_meta */
    if ((k = package_get_item(c->package, "id_meta", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if (!(c->db_id_meta = open_db(path, 0, 0)))
        goto fail;

    /* Creating database md_nrecno */
    if ((k = package_get_item(c->package, "md_nrecno", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if (!(c->db_md_nrecno = open_db(path, 1, 0)))
        goto fail;

    /* Creating database nrecno_md */
    if ((k = package_get_item(c->package, "nrecno_md", 1, &path)) < 0)
        goto fail;
    
    assert(k);
    
    if (!(c->db_nrecno_md = open_db(path, 1, 0)))
        goto fail;

    /* Creating database nrecno_lastmd */
    if ((k = package_get_item(c->package, "nrecno_lastmd", 1, &path)) < 0)
        goto fail;
    
    assert(k);
    
    if (!(c->db_nrecno_lastmd = open_db(path, 0, 0)))
        goto fail;

    /* Creating database md_lastnrecno */
    if ((k = package_get_item(c->package, "md_lastnrecno", 1, &path)) < 0)
        goto fail;
    
    assert(k);
    
    if (!(c->db_md_lastnrecno = open_db(path, 0, 0)))
        goto fail;
    
    /* Creating database version_timestamp */
    if ((k = package_get_item(c->package, "version_timestamp", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if (!(c->db_version_timestamp = open_db(path, 0, 0)))
        goto fail;
    
    /* Creating database nhash_nrecno */
    if ((k = package_get_item(c->package, "nhash_nrecno", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if (!(c->db_nhash_nrecno = open_db(path, 1, 0)))
        goto fail;

    /* Creating database nrecno_name */
    if ((k = package_get_item(c->package, "nrecno_name", 1, &path)) < 0)
        goto fail;

    assert(k);
    
    if (!(c->db_nrecno_name = open_db(path, 0, 1)))
        goto fail;

    return c;

    
fail:
    db_context_free(c);
    
    return NULL;
}

int db_context_save(struct syrep_db_context *c, const char *filename) {
    FILE *f;
    int k;
    const char *path;
    assert(c && c->package);

    if (c->db_id_meta)
        c->db_id_meta->sync(c->db_id_meta, 0);
        
    if (c->db_md_nrecno)
        c->db_md_nrecno->sync(c->db_md_nrecno, 0);

    if (c->db_nrecno_md)
        c->db_nrecno_md->sync(c->db_nrecno_md, 0);

    if (c->db_md_lastnrecno)
        c->db_md_lastnrecno->sync(c->db_md_lastnrecno, 0);

    if (c->db_nrecno_lastmd)
        c->db_nrecno_lastmd->sync(c->db_nrecno_lastmd, 0);

    if (c->db_version_timestamp)
        c->db_version_timestamp->sync(c->db_version_timestamp, 0);

    if (c->db_nhash_nrecno)
        c->db_nhash_nrecno->sync(c->db_nhash_nrecno, 0);
    
    if (c->db_nrecno_name)
        c->db_nrecno_name->sync(c->db_nrecno_name, 0);

    /* Saving timestamp info */
    
    if ((k = package_get_item(c->package, "timestamp", 1, &path)) < 0)
        return -1;

    assert(k);
    
    if (!(f = fopen(path, "w+")))
        return -1;

    fprintf(f, "%i\n", c->timestamp);
    fclose(f);

    /* Save version info */
    
    if ((k = package_get_item(c->package, "version", 1, &path)) < 0)
        return -1;

    assert(k);
    
    if (!(f = fopen(path, "w+")))
        return -1;

    fprintf(f, "%u\n", c->version);
    fclose(f);

    /* Save origin info */
    
    if ((k = package_get_item(c->package, "origin", 1, &path)) < 0)
        return -1;
    
    assert(k);

    if (!(f = fopen(path, "w+")))
        return -1;

    fprintf(f, "%s\n", c->origin);
    fclose(f);
    
    return package_save(c->package, filename);
}

int db_context_origin_warn(struct syrep_db_context *c) {
    char hn[256];

    assert(c);

    if (gethostname(hn, sizeof(hn)) < 0)
        return -1;

    if (strcmp(hn, c->origin)) {
        int q;

        if (!(q = question("WARNING: Snapshot is not from local host! Continue?", "ny")))
            return -1;

        if (q != 'y')
            return 1;
    }

    return 0;
}
