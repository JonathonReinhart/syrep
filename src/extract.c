/* $Id: extract.c 43 2003-11-30 14:27:42Z lennart $ */

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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stdio.h>

#include "package.h"
#include "extract.h"
#include "util.h"

static int cb(struct package *p, const char *name, const char *path, void *u) {
    int r;
    
    if ((r = access(path, R_OK)) < 0) {
        if (errno == ENOENT)
            return 0;

        fprintf(stderr, "stat(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }

    if (r == 0) {
        fprintf(stderr, "Extracting %s ...\n", name);
        return copy_or_link_file(path, name, 1);
    }

    return 0;
}

int extract(struct syrep_db_context *context) {
    return package_foreach(context->package, cb, NULL);
}
