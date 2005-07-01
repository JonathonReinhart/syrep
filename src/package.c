/* $Id: package.c 76 2005-06-05 20:14:45Z lennart $ */

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

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <zlib.h>

#include "package.h"
#include "util.h"
#include "syrep.h"

/* Import mkdtemp */
char *mkdtemp(char *template);

struct package_item;

struct package_item {
    char *name;
    char *path;
    int remove;
    struct package_item *next;
};

#define ZBUFSIZE (64*1024)

struct package {
    char *base;
    int count;
    int read_fd, write_fd;
    z_stream read_z, write_z;
    uint8_t *read_zbuf, *write_zbuf;
    int x_endianess;
    int compressed;
    struct package_item *items;
    struct package_item *last;
};


static int alloc_read_zbuf(struct package *p) {
    if (!p->read_zbuf)
        if (!(p->read_zbuf = malloc(ZBUFSIZE))) {
            fprintf(stderr, "malloc(): %s\n", strerror(errno));
            return -1;
        }

    return 0;
}

static int alloc_write_zbuf(struct package *p) {
    if (!p->write_zbuf)
        if (!(p->write_zbuf = malloc(ZBUFSIZE))) {
            fprintf(stderr, "malloc(): %s\n", strerror(errno));
            return -1;
        }

    return 0;
}

static ssize_t package_read(struct package *p, void *d, size_t l) {
    ssize_t r = -1;
    
    if (!p->compressed) {
        
        if ((r = loop_read(p->read_fd, d, l)) < 0)
            fprintf(stderr, "read(): %s\n", strerror(errno));

        goto finish;
    }
        
    if (!p->read_zbuf && alloc_read_zbuf(p) < 0)
        return -1;    

    p->read_z.next_out = d;
    p->read_z.avail_out = l;

    while (p->read_z.avail_out > 0) {
        int z;
        
        if (p->read_z.avail_in <= 0) {
            ssize_t n;
            
            if ((n = read(p->read_fd, p->read_zbuf, ZBUFSIZE)) < 0) {
                fprintf(stderr, "read(): %s\n", strerror(errno));
                goto finish;
            }

            if (!n) {
                r = (uint8_t*) p->read_z.next_out - (uint8_t*) d;
                goto finish;
            }

            p->read_z.next_in = p->read_zbuf;
            p->read_z.avail_in = n;
        }
        
        if ((z = inflate(&p->read_z, 0)) != Z_OK) {

            if (z == Z_STREAM_END) {
                r = (uint8_t*) p->read_z.next_out - (uint8_t*) d;
                goto finish;
            }
                
            
            fprintf(stderr, "XXX inflate() failed %i\n", z);
            goto finish;
        }
    }

    r = l;

finish:
    
    return r;
}


static ssize_t package_write(struct package *p, void *d, size_t l) {
    ssize_t r = -1;
    
    if (!args.compress_flag) {

        if ((r = loop_write(p->write_fd, d, l)) < 0)
            fprintf(stderr, "write(): %s\n", strerror(errno));

        goto finish;
    }

    if (!p->write_zbuf && alloc_write_zbuf(p) < 0)
        return -1;    
    
    p->write_z.next_in = d;
    p->write_z.avail_in = l;

    while (p->write_z.avail_in > 0) {
        size_t t;

        p->write_z.next_out = p->write_zbuf;
        p->write_z.avail_out = ZBUFSIZE;


        if (deflate(&p->write_z, Z_NO_FLUSH) != Z_OK) {
            fprintf(stderr, "deflate() failed\n");
            goto finish;
        }

        t = (uint8_t*) p->write_z.next_out - p->write_zbuf;

        if (t) {
            ssize_t n;
            
            if ((n = loop_write(p->write_fd, p->write_zbuf, t)) < 0) {
                fprintf(stderr, "loop_write(): %s\n", strerror(errno));
                goto finish;
            }

            if ((size_t) n != t) {
                if ((r = (uint8_t*) p->write_z.next_in - (uint8_t*) d) > 0)
                    r --;

                goto finish;
            }
        }
    }

    r = l;
    
finish:
    
    return r;

}

#define CBUFSIZE ZBUFSIZE
static int copy_deflate(struct package *p, int sfd, off_t l) {
    void *buf = NULL;
    int r = -1;

    if (!(buf = malloc(CBUFSIZE))) {
        fprintf(stderr, "malloc(): %s\n", strerror(errno));
        goto finish;
    }
    
    while (l > 0) {
        size_t t = MIN(l, CBUFSIZE);
        ssize_t n;
        
        if ((n = loop_read(sfd, buf, t)) != (ssize_t) t) {
            fprintf(stderr, "read() : %s\n", n < 0 ? strerror(errno) : "EOF");
            goto finish;
        }

        if (package_write(p, buf, n) != n)
            goto finish;

        l -= n;
    }

    r = 0;

finish:
    if (buf)
        free(buf);
    
    return r;
}

static int copy_inflate(struct package *p, int dfd, off_t l) {
    void *buf = NULL;
    int r = -1;

    if (!(buf = malloc(CBUFSIZE))) {
        fprintf(stderr, "malloc(): %s\n", strerror(errno));
        goto finish;
    }
    
    while (l > 0) {
        size_t t = (size_t) (MIN(l, CBUFSIZE));
        ssize_t n;

        if (package_read(p, buf, t) != (ssize_t) t)
            goto finish;
        
        if ((n = loop_write(dfd, buf, t)) != (ssize_t) t) {
            fprintf(stderr, "write() : %s\n", n < 0 ? strerror(errno) : "EOF");
            goto finish;
        }

        l -= t;
    }

    r = 0;

finish:
    if (buf)
        free(buf);
    
    return r;
}

static char *tmp(char *fn, int l) {
    char *t;

    if (!(t = getenv("TMPDIR")))
        if (!(t = getenv("TEMP")))
            if (!(t = getenv("TMP")))
                t = "/tmp";
    
    snprintf(fn, l, "%s/pkgXXXXXX", t);
    return fn;
}

static struct package_item *item_new(const char *name, const char *path, int r) {
    struct package_item *i = NULL;

    if (!(i = malloc(sizeof(struct package_item)))) {
        fprintf(stderr, "malloc() failed: %s\n", strerror(errno));
        goto fail;
    }

    memset(i, 0, sizeof(struct package_item));

    if (!(i->name = strdup(name))) {
        fprintf(stderr, "strdup() failed: %s\n", strerror(errno));
        goto fail;
    }

    if (!(i->path = strdup(path))) {
        fprintf(stderr, "strdup() failed: %s\n", strerror(errno));
        goto fail;
    }

    i->remove = r;

    return i;

fail:
    if(i) {
        if (i->path)
            free(i->path);
        if (i->name)
            free(i->name);
        free(i);
    }

    return NULL;
}

static void append_item(struct package *p, struct package_item *i) {
    assert(p && i && !!p->last == !!p->items);

    i->next = NULL;
    
    if (p->last) {
        p->last->next = i;
        p->last = i;
    } else
        p->items = p->last = i;
}

static void close_read_fd(struct package *p) {
    assert(p);

    if (p->read_fd >= 0) {
        if (p->compressed)
            inflateEnd(&p->read_z);
        
        if (p->read_fd != STDIN_FILENO)
            close(p->read_fd);
        p->read_fd = -1;
    }
}

static int close_write_fd(struct package *p) {
    int r = 0;
    assert(p);

    if (p->write_fd >= 0) {
        if (args.compress_flag) {
            int z;

            p->write_z.next_in = NULL;
            p->write_z.avail_in = 0;
            
            do {
                size_t t;
                ssize_t n;

                p->write_z.next_out = p->write_zbuf;
                p->write_z.avail_out = ZBUFSIZE;

                z = deflate(&p->write_z, Z_FINISH);

                t = (uint8_t*) p->write_z.next_out - p->write_zbuf;
                if (t) {
                    if ((n = loop_write(p->write_fd, p->write_zbuf, t)) != (ssize_t) t) {
                        fprintf(stderr, "loop_write(): %s\n", n < 0 ? strerror(errno) : "EOF");
                        r = -1;
                        break;
                    }
                }
                
            } while (z == Z_OK);

            if (!r && z != Z_STREAM_END) {
                fprintf(stderr, "Final deflate() failure\n");
                r = -1;
            }
            
            deflateEnd(&p->write_z);
        }

        if (p->write_fd != STDOUT_FILENO)
            close(p->write_fd);
        
        p->write_fd = -1;
    }

    return r;
}

#define X32(i) (((i)<<24)|(((i)<<8)&0x00FF0000)|(((i)>>8)&0x0000FF00)|((i)>>24))
#define X64(i) (X32((i)>>32)|X32((i)>>32))

static int read_item(struct package *p) {
    char name[PACKAGE_ITEMNAMELEN+1];
    char path[PATH_MAX] = "";
    off_t size;
    uint64_t l;
    ssize_t r;
    int fd = -1;
    struct package_item *i;
    
    if (p->read_fd < 0)
        return 0;

    name[PACKAGE_ITEMNAMELEN] = 0;
    if ((r = package_read(p, name, PACKAGE_ITEMNAMELEN)) != PACKAGE_ITEMNAMELEN) {
        if (r == 0) {
            close_read_fd(p);
            return 0;
        } else if (r > 0)
            fprintf(stderr, "Short read\n");
        
        goto fail;
    }

    if ((r = package_read(p, &l, 8)) != 8) {
        if (r >= 0)
            fprintf(stderr, "Short read\n");

        goto fail;
    }
    
    if (p->x_endianess)
        l = X64(l);

    if (args.verbose_flag)
        fprintf(stderr, "Reading %s (%llu) from package.\n", name, l);

    size = (off_t) l;
    
    snprintf(path, sizeof(path), "%s/%i", p->base, p->count++);

    if ((fd = open(path, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0) {  /* RDWR for mmap compatibility */
        fprintf(stderr, "open(\"%s\"): %s\n", path, strerror(errno));
        goto fail;
    }
    
    if (!p->compressed) {
        if (copy_fd(p->read_fd, fd, size) < 0)
            goto fail;
    } else {
        if (copy_inflate(p, fd, size) < 0)
            goto fail;
    }

    if (!(i = item_new(name, path, 1)))
        goto fail;

    append_item(p, i);
    
    close(fd);
    
    return 1;

fail:

    if (fd >= 0)
        close(fd);

    if (path[0])
        unlink(path);
    
    return -1;
}


static int write_item(struct package *p, struct package_item *i) {
    char name[PACKAGE_ITEMNAMELEN+1];
    ssize_t r;
    int fd = -1;
    off_t size;
    uint64_t l;

    if ((fd = open(i->path, O_RDONLY)) < 0) {
        if (errno == ENOENT)
            return 0;

        fprintf(stderr, "open(\"%s\"): %s\n", i->path, strerror(errno));
        goto fail;
    }
        
    if ((size = filesize(fd)) == (off_t) -1)
        return -1;

    l = (uint64_t) size;

    memset(name, 0, sizeof(name));
    strncpy(name, i->name, PACKAGE_ITEMNAMELEN);
    
    if (args.verbose_flag)
        fprintf(stderr, "Writing %s (%llu bytes) to package.\n", i->name, l);

    if ((r = package_write(p, name, PACKAGE_ITEMNAMELEN)) != PACKAGE_ITEMNAMELEN) {
        if (r >= 0)
            fprintf(stderr, "Short write\n");
        
        goto fail;
    }

    if ((r = package_write(p, &l, 8)) != 8) {
        if (r >= 0)
            fprintf(stderr, "Short write\n");

        goto fail;
    }

    if (size) {
        if (!args.compress_flag) {
            if (copy_fd(fd, p->write_fd, size) < 0)
                goto fail;
        } else {
            if (copy_deflate(p, fd, size) < 0)
                goto fail;
        }
    }

    close(fd);
    return 0;
    
fail:

    if (fd > 0)
        close(fd);

    return -1;
}

struct package* package_open(const char *fn, int force) {
    struct package *p;
    char path[PATH_MAX];

    if (!(p = malloc(sizeof(struct package)))) {
        fprintf(stderr, "Failed to allocate package structure.\n");
        return NULL;
    }

    memset(p, 0, sizeof(struct package));
    p->read_fd = p->write_fd = -1;

    tmp(path, sizeof(path));
    if (!mkdtemp(path)) {
        fprintf(stderr, "Failed to create temporary directory.\n");
        goto finish;
    }

    if (!(p->base = strdup(path))) {
        fprintf(stderr, "Failed to allocate memory.\n");
        goto finish;
    }
    
    if (fn) {
        if (!strcmp(fn, "-"))
            p->read_fd = STDIN_FILENO;
        else if ((p->read_fd = open(fn, O_RDONLY)) < 0) {
            
            if (errno != ENOENT || !force) {
                fprintf(stderr, "Failed to open <%s>: %s\n", fn, strerror(errno));
                goto finish;
            }
        }

        if (p->read_fd >= 0) {
            uint32_t id;
            ssize_t r;
            
            if ((r = loop_read(p->read_fd, &id, 4)) != 4) {
                fprintf(stderr, "read(): %s\n", r < 0 ? strerror(errno) : "EOF");
                
                goto finish;
            }

            if (id == PACKAGE_FILEID)
                p->x_endianess = p->compressed = 0;
            else if (id == X32(PACKAGE_FILEID))
                p->x_endianess = !(p->compressed = 0);
            else if (id == PACKAGE_FILEIDCOMPRESSED)
                p->x_endianess = !(p->compressed = 1);
            else if (id == X32(PACKAGE_FILEIDCOMPRESSED))
                p->x_endianess = p->compressed = 1;
            else {
                fprintf(stderr, "%s is not a syrep snapshot or patch\n", fn);
                goto finish;
            }
        }
        
    }

    if (p->compressed) {

        memset(&p->read_z, 0, sizeof(p->read_z));
        
        if (inflateInit(&p->read_z) != Z_OK) {
            fprintf(stderr, "zlib initialisation failure\n");
            goto finish;
        }
    }
    

    return p;

finish:

    if (p)
        package_remove(p);

    return NULL;
}

static int load_complete(struct package *p) {
    int r;
    
    assert(p);
    
    if (p->read_fd < 0)
        return 0;

    while ((r = read_item(p)) > 0) {
        rotdash();

        if (interrupted) {
            rotdash_hide();
            fprintf(stderr, "Canceled.\n");
            return -1;
        }
    }
        
    rotdash_hide();
    
    return r;
}

int package_save(struct package *p, const char *fn) {
    char path[PATH_MAX] = "";
    int r = -1;
    struct package_item *i;
    uint32_t id;
    ssize_t n;
    
    assert(p && p->write_fd < 0);

    if (load_complete(p) < 0)
        goto finish;

    if (!fn || !strcmp(fn, "-"))
        p->write_fd = STDOUT_FILENO;
    else {
        char hn[256];

        if (gethostname(hn, sizeof(hn)) < 0) {
            fprintf(stderr, "gethostname(): %s\n", strerror(errno));
            goto finish;
        }

        snprintf(path, sizeof(path), "%s.%s.%u", fn, hn, getpid());

        if ((p->write_fd = open(path, O_RDWR|O_TRUNC|O_CREAT, 0666)) < 0) {  /* RDWR for mmap compatibility */
            fprintf(stderr, "open(\"%s\"): %s\n", fn, strerror(errno));
            goto finish;
        }
    }

    id = args.compress_flag ? PACKAGE_FILEIDCOMPRESSED : PACKAGE_FILEID;

    if ((n = loop_write(p->write_fd, &id, 4)) != 4) {
        if (n < 0)
            fprintf(stderr, "write(): %s\n", r < 0 ? strerror(errno) : "EOF");

        goto finish;
    }

    if (args.compress_flag) {

        memset(&p->write_z, 0, sizeof(p->write_z));
        
        if (deflateInit(&p->write_z, Z_DEFAULT_COMPRESSION) != Z_OK) {
            fprintf(stderr, "zlib initialisation failure\n");
            goto finish;
        }
    }
    
    for (i = p->items; i; i = i->next) {
        
        if (!i->name || !i->path)
            continue;

        if (write_item(p, i) < 0)
            goto finish;

        rotdash();

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            goto finish;
        }
    }
    
    r = 0;

finish:

    if (close_write_fd(p) < 0)
        r = -1;

    if (!r) {
        unlink(fn);
        if (link(path, fn) < 0) {
            fprintf(stderr, "link(): %s\n", strerror(errno));
            r = -1;
        }
    }

    if (path[0])
        unlink(path);

    rotdash_hide();

    return r;
}

int package_get_item(struct package* p, const char *name, int c, const char ** fn) {
    struct package_item *i;
    char path[PATH_MAX];
    assert(p && name);

    for (i = p->items; i; i = i->next)
        if (!strncmp(name, i->name, PACKAGE_ITEMNAMELEN)) {
            if (fn)
                *fn = i->path;

            return 1;
        }

    for (;;) {
        int r;
        
        if ((r = read_item(p)) < 0)
            return -1;

        if (!r)
            break;

        assert(p->last);
        if (!strncmp(name, p->last->name, PACKAGE_ITEMNAMELEN)) {
            if (fn)
                *fn = p->last->path;
            
            return 1;
        }
    }

    if (!c)
        return 0;
    
    snprintf(path, sizeof(path), "%s/%i", p->base, p->count++);
    
    if (!(i = item_new(name, path, 1))) {
        unlink(path);
        return -1;
    }
    
    append_item(p, i);

    assert(i);

    assert(fn);
    
    if (fn)
        *fn = i->path;
    
    return 1;
}

int package_add_file(struct package *p, const char *name, const char *fn) {
    struct package_item *i;

    if (!(i = item_new(name, fn, 0)))
        return -1;
    
    append_item(p, i);
    
    return 0;
}

void package_remove(struct package *p) {
    struct package_item *i, *n;
    assert(p);

    for (i = p->items; i; i = n) {
        n = i->next;

        if (i->remove && i->path)
            if (unlink(i->path))
                if (errno != ENOENT)
                    fprintf(stderr, "Failed to remove <%s>: %s\n", i->path, strerror(errno));
        
        free(i->path);
        free(i->name);
        free(i);
    }

    if (p->base) {
        if (rmdir(p->base)) {
            if (errno != ENOENT)
                fprintf(stderr, "Failed to remove <%s>: %s\n", p->base, strerror(errno));
        }
        
        free(p->base);
    }

    close_read_fd(p);
    close_write_fd(p);

    if (p->read_zbuf)
        free(p->read_zbuf);
    
    if (p->write_zbuf)
        free(p->write_zbuf);
    
    free(p);
}


int package_foreach(struct package *p, int (*cb) (struct package *p, const char *name, const char *path, void *u), void *u) {
    struct package_item *i;
    char rname[PACKAGE_ITEMNAMELEN+1];
    assert(p);
    
    for (i = p->items; i; i = i->next) {
        memset(rname, 0, sizeof(rname));
        strncpy(rname, i->name, PACKAGE_ITEMNAMELEN);
        if (cb(p, rname, i->path, u) < 0)
            return -1;
    }
    
    for (;;) {
        int r;
        
        if ((r = read_item(p)) < 0)
            return -1;

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            return -1;
        }

        if (r == 0)
            break;

        assert(p->last);

        memset(rname, 0, sizeof(rname));
        strncpy(rname, p->last->name, PACKAGE_ITEMNAMELEN);

        if (cb(p, rname, p->last->path, u) < 0)
            return -1;
    }
    
    return 0;
}
