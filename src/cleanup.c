/* $Id: cleanup.c 43 2003-11-30 14:27:42Z lennart $ */

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
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "syrep.h"
#include "util.h"

int cleanup(const char *root) {
    char p[PATH_MAX];
    
    if (args.cleanup_level_arg >= 1) {

        if (args.verbose_flag) 
            fprintf(stderr, "Emptying trash ...\n");
        
        snprintf(p, sizeof(p), "%s/.syrep/" SYREP_TRASHDIR, root);
        
        if (rm_rf(p, 0) < 0)
            return -1;

        if (args.verbose_flag) 
            fprintf(stderr, "Removing temporary directories  ...\n");

        snprintf(p, sizeof(p), "%s/.syrep/" SYREP_TEMPDIR, root);

        if (rm_rf(p, 1) < 0)
            return -1;
    }

    if (args.cleanup_level_arg >= 2) {

        if (args.verbose_flag) 
            fprintf(stderr, "Removing digest cache ...\n");
        
        snprintf(p, sizeof(p), "%s/.syrep/" SYREP_MDCACHEFILENAME, root);

        if (unlink(p) < 0) {
            if (errno != ENOENT) {
                fprintf(stderr, "unlink(\"%s\"): %s\n", p, strerror(errno));
                return -1;
            }
        }
    }

    if (args.cleanup_level_arg >= 3) {
        
        if (args.verbose_flag)
            fprintf(stderr, "Removing status data ...\n");

        snprintf(p, sizeof(p), "%s/.syrep/" SYREP_SNAPSHOTFILENAME, root);

        if (unlink(p) < 0) {
            if (errno != ENOENT) {
                fprintf(stderr, "unlink(\"%s\"): %s\n", p, strerror(errno));
                return -1;
            }
        }

        snprintf(p, sizeof(p), "%s/.syrep", root);

        if (rmdir(p) < 0) {
            if (errno != ENOENT) {
                fprintf(stderr, "rmdir(\"%s\"): %s\n", p, strerror(errno));
                return -1;
            }
        }
    }


    return 0;
}
