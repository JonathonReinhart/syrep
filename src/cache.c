/* $Id: cache.c 43 2003-11-30 14:27:42Z lennart $ */

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

#include <db.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cache.h"
#include "md5util.h"

struct syrep_cache_key {
    uint64_t dev;
    uint64_t inode;
    uint32_t date;
    uint64_t size;
};

struct syrep_cache_data {
    uint8_t digest[16];
    uint32_t timestamp;
};

struct syrep_md_cache {
    DB* db;
    uint32_t timestamp;
    int ro;
};

struct syrep_md_cache* md_cache_open(const char* fn, int ro) {
    struct syrep_md_cache *c = NULL;
    int ret;
    
    assert(fn);

    if (!(c = malloc(sizeof(struct syrep_md_cache))))
        goto fail;

    c->timestamp = time(NULL);
    c->ro = ro;

    if ((ret = db_create(&c->db, NULL, 0))) {
        fprintf(stderr, "db_create: %s\n", db_strerror(ret));
        goto fail;
    }

    if ((ret = c->db->open(c->db, NULL, fn, NULL, DB_BTREE, ro ? DB_RDONLY : DB_CREATE, 0664))) {
        c->db->err(c->db, ret, "open(%s)", fn);
        goto fail;
    }

    return c;

fail:

    if (c) {
        if (c->db)
            c->db->close(c->db, 0);

        free(c);
    }

    return NULL;
}

void md_cache_close(struct syrep_md_cache *c) {
    assert(c);

    if (c->db)
        c->db->close(c->db, 0);

    free(c);
}

static int get(struct syrep_md_cache *c, const struct syrep_cache_key *k, uint8_t digest[16]) {
    int ret;
    DBT key, data;

    assert(c && c->db && k);
    
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = (void*) k;
    key.size = sizeof(struct syrep_cache_key);
    
    if ((ret = c->db->get(c->db, NULL, &key, &data, 0)) != 0) {
        if (ret == DB_NOTFOUND) {
            //fprintf(stderr, "Cache MISS!\n");
            return 0;
        }
        
        c->db->err(c->db, ret, "cache::get");
        return -1;
    }

    //fprintf(stderr, "Cache HIT\n");

    assert(data.data);
    memcpy(digest, &((struct syrep_cache_data*) data.data)->digest, 16);
    
    return 1;
}

static int put(struct syrep_md_cache *c, const struct syrep_cache_key *k, const uint8_t digest[16], uint32_t timestamp) {
    int ret;
    DBT key, data;
    struct syrep_cache_data d;

    assert(c && c->db && k);
    
    memset(&key, 0, sizeof(key));
    key.data = (void*) k;
    key.size = sizeof(struct syrep_cache_key);

    memcpy(d.digest, digest, 16);
    d.timestamp = timestamp;
    
    memset(&data, 0, sizeof(data));
    data.data = &d;
    data.size = sizeof(struct syrep_cache_data);
        
    if ((ret = c->db->put(c->db, NULL, &key, &data, 0)) != 0) {
        c->db->err(c->db, ret, "cache::put");
        return -1;
    }

    return 0;
}

int md_cache_get(struct syrep_md_cache *c, const char *path, uint8_t digest[16]) {
    struct syrep_cache_key k;
    int r = -1, fd = -1, j;
    struct stat st;
    
    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "open(\"%s\"): %s\n", path, strerror(errno));
        goto finish;
    }
    
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "fstat(%s): %s\n", path, strerror(errno));
        goto finish;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "<%s> not a regular file: %s\n", path, strerror(errno));
        goto finish;
    }

    memset(&k, 0, sizeof(k));
    k.dev = (uint64_t) st.st_dev;
    k.inode = (uint64_t) st.st_ino;
    k.date = (uint32_t) st.st_mtime;
    k.size = (uint64_t) st.st_size;

    if (!c)
        j = 0;
    else
        if ((j = get(c, &k, digest)) < 0)
            goto finish;
    
    if (!j)
        if (fdmd5(fd, st.st_size, digest) < 0)
            goto finish;

    if (c && !c->ro)
        put(c, &k, digest, c->timestamp);

    r = 0;

finish:

    if (fd >= 0)
         close(fd);
    
    return r;
}

int md_cache_vacuum(struct syrep_md_cache*c) {
    int r = -1, ret;
    DBC *cursor;
    DBT key, data;
    int ndel = 0, ntotal = 0;

    if (c->ro)
        return 0;

    assert(c && c->db);
    
    if ((ret = c->db->cursor(c->db, NULL, &cursor, 0)) != 0) {
        c->db->err(c->db, ret, "cache::vacuum");
        return -1;
    }

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    
    while ((ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0) {
        struct syrep_cache_data *d;

        ntotal++;
        d = (struct syrep_cache_data*) data.data;
        assert(d);
        if (d->timestamp < c->timestamp) {
            cursor->c_del(cursor, 0);
            ndel++;
        }
    }

    if (ret != DB_NOTFOUND) {
        c->db->err(c->db, ret, "cache::vacuum()");
        goto finish;
    }

    /*fprintf(stderr, "Cache vacuum successfully completed, %i of %i entries deleted.\n", ndel, ntotal);*/
    
    r = 0;

finish:

    cursor->c_close(cursor);

    return r;
}
