/* $Id: merge.c 43 2003-11-30 14:27:42Z lennart $ */

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
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "md5util.h"
#include "diff.h"
#include "dbutil.h"
#include "package.h"
#include "util.h"

struct cb_info {
    struct syrep_db_context *c1; /* remote */
    struct syrep_db_context *c2; /* local */
    const char *root;  /* directory wherein to merge files */
    const char *sroot;  /* An optional source directory for bi-directory merges */
    char trash_dir[PATH_MAX];
};

static int conflict_phase(DB *ddb, struct syrep_name *name, struct diff_entry *de, void *p) {
    struct cb_info *cb_info = p;
    struct syrep_md md1, md2;
    struct syrep_nrecno nrecno1, nrecno2;
    int writeback = 0;
    int f1, f2;
    char path[PATH_MAX];

    assert(ddb && name && de && p);

    if (de->action != DIFF_CONFLICT)
        return 0;

    if ((f1 = get_nrecno_by_name(cb_info->c1, name, &nrecno1, 0)) < 0)
        return -1;

    if (f1)
        if ((f1 = get_current_md_by_nrecno(cb_info->c1, &nrecno1, &md1)) < 0)
            return -1;

    if ((f2 = get_nrecno_by_name(cb_info->c2, name, &nrecno2, 0)) < 0)
        return -1;

    if (f2)
        if ((f2 = get_current_md_by_nrecno(cb_info->c2, &nrecno2, &md2)) < 0)
            return -1;
    
    snprintf(path, sizeof(path), "%s/%s", cb_info->root, name->path);

    if (f1 && f2) {
        char d1[SYREP_DIGESTLENGTH*2+1], d2[SYREP_DIGESTLENGTH*2+1];
        int q;
        
        fhex(md1.digest, SYREP_DIGESTLENGTH, d1);
        fhex(md2.digest, SYREP_DIGESTLENGTH, d2);
        d1[SYREP_DIGESTLENGTH*2] = d2[SYREP_DIGESTLENGTH*2] = 0;
        
        fprintf(stderr, "CONFLICT: New file '%s': %s in *local* vs. %s in *remote* repository.\n", path, d2, d1);

        if (!(q = question("Replace in local repository?", "ny")))
            return -1;
        
        if (q == 'y') {
            de->action = DIFF_REPLACE;
            de->repository = cb_info->c1;
            writeback = 1;
        }
        
    } else if (f1 || f2) {
        char d[SYREP_DIGESTLENGTH*2+1];
        
        fhex(f1 ? md1.digest : md2.digest, SYREP_DIGESTLENGTH, d);
        d[SYREP_DIGESTLENGTH*2] = 0;

        fprintf(stderr, "CONFLICT: File '%s' (%s) appeared in *%s*, removed in *%s* repository.\n", path, d, f1 ? "remote" : "local", f2 ? "remote" : "local");

        if (f1) {
            int q;
            
            if (!(q = question("Add to local repository?", "yn")))
                return -1;
            
            if (q == 'y') {
                de->action = DIFF_COPY;
                de->repository = cb_info->c1;
                writeback = 1;
            }
        } else {
            int q;
            
            if (!(q = question("Remove from local repository?", "ny")))
                return -1;

            if (q == 'y') {
                de->action = DIFF_DELETE;
                de->repository = cb_info->c2;
                writeback = 1;
            }
        }
    }

    if (writeback) {
        DBT key, data;
        int ret;

        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));

        key.data = (void*) name;
        key.size = sizeof(struct syrep_name);

        data.data = (void*) de;
        data.size = sizeof(struct diff_entry);
        
        if ((ret = ddb->put(ddb, NULL, &key, &data, 0))) {
            ddb->err(ddb, ret, "ddb::put()");
            return -1;
        }
    }
    
    return 0;
}

static char *escape_path(const char *path, char *dst, unsigned l) {
    const char *p;
    char *d;
    
    for (p = path, d = dst; *p && d-dst < l-1; p++) {
        if (*p == '/') {
            *(d++) = '%';
            *(d++) = '2';
            *(d++) = 'F';

        } else if (*p == '%') {
            *(d++) = '%';
            *(d++) = '2';
            *(d++) = '5';

        } else
            *(d++) = *p;
    }

    *(d++) = 0;
    return dst;
}

static int copy_phase(DB *ddb, struct syrep_name *name, struct diff_entry *de, void *p) {
    struct cb_info *cb_info = p;
    struct syrep_name name2;
    struct syrep_nrecno nrecno, nrecno2;
    struct syrep_md md;
    char path[PATH_MAX];
    int f;
    char d[SYREP_DIGESTLENGTH*2+1];

    int (*copy_proc) (const char *, const char*, int c) = args.always_copy_flag ? copy_file : copy_or_link_file;
        

    assert(ddb && name && de && p);

    if (de->action != DIFF_COPY && de->action != DIFF_REPLACE)
        return 0;

    if (de->repository == cb_info->c2)
        return 0;

    snprintf(path, sizeof(path), "%s/%s", cb_info->root, name->path);

    if ((f = get_nrecno_by_name(cb_info->c1, name, &nrecno, 0)) < 0)
        return -1;

    if (f)
        if ((f = get_current_md_by_nrecno(cb_info->c1, &nrecno, &md)) < 0)
            return -1;

    if (!f) {
        fprintf(stderr, "Diff invalid!\n");
        return -1;
    }

    fhex(md.digest, SYREP_DIGESTLENGTH, d);
    d[SYREP_DIGESTLENGTH*2] = 0;

    if ((f = get_current_nrecno_by_md(cb_info->c2, &md, &nrecno2)) < 0)
        return -1;

    if (f)
        if ((f = get_name_by_nrecno(cb_info->c2, &nrecno2, &name2)) < 0)
            return -1;
    
    if (f) {
        char path2[PATH_MAX];
        snprintf(path2, sizeof(path2), "%s/%s", cb_info->root, name2.path);

        if (args.verbose_flag)
            fprintf(stderr, "COPY: Linking existing file <%s> to <%s>.\n", name2.path, name->path);

        if (makeprefixpath(path, 0777) < 0)
            return -1;

        if (!access(path2, R_OK)) {
            if (copy_proc(path2, path, 0) < 0)
                return -1;
        } else {
            unsigned l;

            snprintf(path2, sizeof(path2), "%s/", cb_info->trash_dir);
            l = strlen(path2);
            escape_path(name2.path, path2+l, sizeof(path2)-l);

            if (!access(path2, R_OK)) {
                if (copy_proc(path2, path, de->action == DIFF_REPLACE) < 0)
                    return -1;
            } else
                fprintf(stderr, "COPY: Local file <%s> vanished. Snapshot not up to date.\n", name2.path);
        }
        
    } else {
        char spath[PATH_MAX];
        const char* a;
        const char* t;
        int k;

        if ((k = package_get_item(cb_info->c1->package, d, 0, &a)) < 0)
            return -1;

        t = "patch";
        
        if (!k && cb_info->sroot) {
            snprintf(spath, sizeof(spath), "%s/%s", cb_info->sroot, name->path);
            a = spath;

            if ((k = (access(spath, R_OK) == 0)))
                t = "tree";
        }
        
        if (k) {
            if (args.verbose_flag)
                fprintf(stderr, "COPY: Copying file <%s> from %s.\n", name->path, t);
            
            if (makeprefixpath(path, 0777) < 0)
                return -1;
            
            if (copy_proc(a, path, de->action == DIFF_REPLACE) < 0)
                return -1;
        } else
            if (args.verbose_flag)
                fprintf(stderr, "COPY: File <%s> is missing.\n", name->path);
    }

    return 0;
}

static int delete_phase(DB *ddb, struct syrep_name *name, struct diff_entry *de, void *p) {
    struct cb_info *cb_info = p;
    char path[PATH_MAX], target[PATH_MAX];
    unsigned l;

    assert(ddb && name && de && p);
    
    if (de->action != DIFF_DELETE && de->action != DIFF_REPLACE)
        return 0;

    if (de->action == DIFF_DELETE && de->repository == cb_info->c1)
        return 0;

    if (de->action == DIFF_REPLACE && de->repository == cb_info->c2)
        return 0;

    snprintf(path, sizeof(path), "%s/%s", cb_info->root, name->path);

    if (args.check_md_flag) {
        uint8_t digest[16];
        struct syrep_md md;
        
        if (fmd5(path, digest) < 0) {
            fprintf(stderr, "Unable to calculate message digest sum on '%s'\n", name->path);
            return -1;
        }

        if (get_current_md_by_name(cb_info->c2,
                                   name, &md) < 0) {
            fprintf(stderr, "Failed to get current MD by name of '%s'\n", name->path);
            return -1;
        }

        if (memcmp(md.digest, digest, 16) != 0) {
            fprintf(stderr, "Message digest of file to delete doesn't match snapshot data for '%s'.\n", name->path);
            return -1;
        }
    }
    
    if (args.question_flag) {
        char text[256];
        int q;

        snprintf(text, sizeof(text), "Delete %s?", name->path);
        
        if ((q = question(text, "yn")) < 0)
            return -1;

        if (q != 'y')
            return 0;
    }
        
    snprintf(target, sizeof(target), "%s/", cb_info->trash_dir);
    l = strlen(target);
    escape_path(name->path, target+l, sizeof(target)-l);
    
    fprintf(stderr, "DELETE: Moving file <%s> into trash (%s)\n", path, target);

    if (move_file(path, target, 1) < 0)
        return -1;

    if (args.prune_empty_flag)
        if (prune_empty_directories(path, cb_info->root) < 0)
            return -1;

    return 0;
}

/* Merges c1 into c2 in directory "root"
 * sroot is an optional source directory, only used on bi-directory merges */
int merge(struct syrep_db_context *c1, struct syrep_db_context *c2, const char* root, const char* sroot) {
    struct cb_info cb_info;
    DB *ddb = NULL;
    int r = -1;

    memset(&cb_info, 0, sizeof(cb_info));
    cb_info.c1 = c1;
    cb_info.c2 = c2;
    cb_info.root = root;
    cb_info.sroot = sroot;

    snprintf(cb_info.trash_dir, sizeof(cb_info.trash_dir), "%s/.syrep/trash", root);
    mkdir_p(cb_info.trash_dir, 0777);

    if (!(ddb = make_diff(c1, c2)))
        goto finish;

    if (diff_foreach(ddb, conflict_phase, &cb_info) < 0)
        goto finish;

    if (diff_foreach(ddb, delete_phase, &cb_info) < 0)
        goto finish;

    if (diff_foreach(ddb, copy_phase, &cb_info) < 0)
        goto finish;

    if (!args.keep_trash_flag)
        if (rm_rf(cb_info.trash_dir, 0) < 0)
            goto finish;
    
    r = 0;
    
finish:
    if (ddb)
        ddb->close(ddb, 0);

    rmdir(cb_info.trash_dir);
    
    return r;
}

