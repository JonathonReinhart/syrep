/* $Id: makepatch.c 32 2003-09-07 23:11:37Z lennart $ */

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

#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "package.h"
#include "makepatch.h"
#include "diff.h"
#include "dbutil.h"
#include "md5util.h"

struct cb_info {
    struct syrep_db_context *c1;
    struct syrep_db_context *c2;
    const char *root;
};

static int cb(DB *ddb, struct syrep_name *name, struct diff_entry *de, void *p) {
    struct cb_info *cb_info = p;
    struct syrep_md md;
    struct syrep_nrecno nrecno;
    char path[PATH_MAX+1];
    char d[SYREP_DIGESTLENGTH*2+1];
    int f, k;

    assert(ddb && name && de && p);

    if (de->action != DIFF_COPY && de->action != DIFF_CONFLICT)
        return 0;

    /* Check whether file exists in c1. If not, exit */
    
    if ((f = get_nrecno_by_name(cb_info->c1, name, &nrecno, 0)) < 0)
        return -1;

    if (f)
        if ((f = get_current_md_by_nrecno(cb_info->c1, &nrecno, &md)) < 0)
            return -1;

    if (!f)
        return 0;

    /* Omit if the file exists in c2 under a different name */

    if (!args.include_all_flag) {
    
        if ((f = get_current_nrecno_by_md(cb_info->c2, &md, NULL)) < 0)
            return -1;

        if (f)
            return 0;
    }
        
    /* Ok, we add this file to the patch */

    fhex(md.digest, SYREP_DIGESTLENGTH, d);
    d[SYREP_DIGESTLENGTH*2] = 0;

    snprintf(path, sizeof(path), "%s/%s", cb_info->root, name->path);

    if ((k = package_get_item(cb_info->c1->package, d, 0, NULL)) < 0)
        return -1;

    if (!k) {

        if (args.verbose_flag)
            fprintf(stderr, "Adding %s (%s) to patch.\n", name->path, d);
        
        if (package_add_file(cb_info->c1->package, d, path) < 0)
            return -1;
    }

    return 0;
}

int makepatch(struct syrep_db_context *c1, struct syrep_db_context *c2, const char *root) {
    struct cb_info cb_info;
    DB *ddb = NULL;
    int r = -1;

    memset(&cb_info, 0, sizeof(cb_info));
    cb_info.c1 = c1;
    cb_info.c2 = c2;
    cb_info.root = root;

    if (!(ddb = make_diff(c1, c2)))
        goto finish;

    if (diff_foreach(ddb, cb, &cb_info) < 0)
        goto finish;
    
    r = 0;
        
finish:
    if (ddb)
        ddb->close(ddb, 0);
    
    return r;
}
