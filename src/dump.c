/* $Id: dump.c 38 2003-09-09 17:06:08Z lennart $ */

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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "dump.h"
#include "package.h"

static int foreach(struct package *p, const char *name, const char *path, void *u) {
    struct stat st;
    uint32_t size;

    if (stat(path, &st) < 0) {
        if (errno == ENOENT)
            size = 0;
        else {
            fprintf(stderr, "stat(%s) failed: %s\n", path, strerror(errno));
            return -1;
        }
    } else
        size = (uint32_t) st.st_size;
    
    printf("%s (%u bytes)\n", name, size);
    return 0;
}

int dump(struct syrep_db_context *c) {
    assert(c);

    return package_foreach(c->package, foreach, NULL);
}
