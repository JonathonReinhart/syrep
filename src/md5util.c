/* $Id: md5util.c 61 2004-07-19 17:09:25Z lennart $ */

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

#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "md5util.h"
#include "md5.h"
#include "syrep.h"

void fhex(const unsigned char *bin, int len, char *txt) {
    static const char hex[] = "0123456789abcdef";
    int i;

    for (i = 0; i < len; i++) {
        txt[i*2] = hex[bin[i]>>4];
        txt[i*2+1] = hex[bin[i]&0xF];
    }
}

#define MMAPSIZE (100*1024*1024)
#define BUFSIZE (1024*1024)

int fdmd5(int fd, off_t l, char *md) {
    void *d;
    off_t o = 0;
    size_t m;
    int r = -1;
    md5_state_t s;
    struct stat pre, post;
    void *p = NULL;

    md5_init(&s);

    if (fstat(fd, &pre) < 0) {
        fprintf(stderr, "fstat(): %s\n", strerror(errno));
        goto finish;
    }

    if (l == (off_t) -1)
        l = pre.st_size;
    
    if (l > BUFSIZE) {
    
        m = l < MMAPSIZE ? l : MMAPSIZE;

        while (l && ((d = mmap(NULL, m, PROT_READ, MAP_SHARED, fd, o)) != MAP_FAILED)) {
            md5_append(&s, d, m);
            munmap(d, m);

            if (interrupted) {
                fprintf(stderr, "Canceled.\n");
                goto finish;
            }
            
            o += m;
            l -= m;
            m = l < MMAPSIZE ? l : MMAPSIZE;
            
        }


        if (l > 0)
            fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
    }

    if (l > 0) {
        if (!(p = malloc(BUFSIZE))) {
            fprintf(stderr, "malloc(): %s\n", strerror(errno));
            goto finish;
        }
        
        while (l) {
            ssize_t r;
            
            if ((r = read(fd, p, BUFSIZE)) < 0) {
                fprintf(stderr, "read(): %s\n", strerror(errno));
                goto finish;
            }
            
            if (!r)
                break;
            
            md5_append(&s, p, r);

            l -= r;
        }
    }

    if (fstat(fd, &post) < 0) {
        fprintf(stderr, "fstat(): %s\n", strerror(errno));
        goto finish;
    }

    if (pre.st_mtime != post.st_mtime) {
        fprintf(stderr, "File modified while calculating digest.\n");
        goto finish;
    }

    md5_finish(&s, md);

    r = 0;

finish:

    if (p)
        free(p);
    
    return r;
}

int fmd5(const char *fn, char *md) {
    int fd = -1, r = -1;
    
    if ((fd = open(fn, O_RDONLY)) < 0) {
        fprintf(stderr, "open(\"%s\"): %s\n", fn, strerror(errno));
        goto finish;
    }

    r = fdmd5(fd, (off_t) -1, md);

finish:

    if (fd >= 0)
        close(fd);

    return r;
}
