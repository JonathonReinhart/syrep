/* $Id: copy-fd-test.c 76 2005-06-05 20:14:45Z lennart $ */

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
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "util.h"
#include "cmdline.h"

struct gengetopt_args_info args;

int interrupted;

int main(int argc, char *argv[]) {
    int fda = -1, fdb = -1, r = -1;
    off_t l;
    
    assert(argc >= 3);

    if ((fda = open(argv[1], O_RDONLY)) < 0 ||
        (fdb = open(argv[2], O_RDWR|O_EXCL|O_CREAT, 0666)) < 0) {
        fprintf(stderr, "open(): %s\n", strerror(errno));
        goto finish;
    }

    if ((l = filesize(fda)) == (off_t) -1) {
        fprintf(stderr, "filesize(): %s\n", strerror(errno));
        goto finish;
    }

    if (copy_fd(fda, fdb, l) < 0) {
        fprintf(stderr, "copy_fd(): %s\n", strerror(errno));
        goto finish;
    }

    r = 0;
    

finish:

    if (fda >= 0)
        close(fda);

    if (fdb >= 0)
        close(fdb);
    
    return r;
}
