/* $Id: syrep.c 108 2006-04-26 13:01:19Z lennart $ */

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

#include <inttypes.h>
#include <limits.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <db.h>
#include <zlib.h>

#include "md5util.h"
#include "md5.h"
#include "context.h"
#include "cache.h"
#include "update.h"
#include "list.h"
#include "diff.h"
#include "util.h"
#include "cmdline.h"
#include "syrep.h"
#include "merge.h"
#include "info.h"
#include "history.h"
#include "dump.h"
#include "extract.h"
#include "makepatch.h"
#include "cleanup.h"
#include "forget.h"

#include "svn-revision.h"

volatile int interrupted = 0;

struct gengetopt_args_info args;

static int do_diff(void) {
    struct syrep_db_context *c1 = NULL, *c2 = NULL;
    char *path1 = NULL, *path2 = NULL;
    DB *ddb = NULL;
    int r = 1;

    if (args.inputs_num != 2) {
        fprintf(stderr, "ERROR: Need exactly two repository snapshots for diff command\n");
        goto finish;
    }

    if (args.local_temp_flag && isdirectory(args.inputs[0]) >= 1) {
        const char *p = get_attached_filename(args.inputs[0], SYREP_TEMPDIR);
        mkdir(p, 0777);
        set_tmp_dir(p);
    }

    if (!(path1 = strdup(get_snapshot_filename(args.inputs[0], SYREP_SNAPSHOTFILENAME))))
        goto finish;
    
    if (!(path2 = strdup(get_snapshot_filename(args.inputs[1], SYREP_SNAPSHOTFILENAME))))
        goto finish;
    
    if (!strcmp(path1, path2)) {
        fprintf(stderr, "ERROR: diff command requires two distinct snapshots as arguments\n");
        goto finish;
    }

    if (!(c1 = db_context_open(path1, 0)))
        goto finish;
    
    if (!(c2 = db_context_open(path2, 0)))
        goto finish;
    
    if (!(ddb = make_diff(c1, c2)))
        goto finish;

    if (list_diff(c1, c2, ddb, isdirectory(args.inputs[0]) > 0 ? args.inputs[0] : NULL, isdirectory(args.inputs[1]) > 0 ? args.inputs[1] : NULL) < 0)
        goto finish;

    r = 0;
    
finish:
    if (c1)
        db_context_free(c1);

    if (c2)
        db_context_free(c2);

    if (ddb)
        ddb->close(ddb, 0);

    if (path1)
        free(path1);

    if (path2)
        free(path2);

    return r;
}

static int do_merge(void) {
    struct syrep_db_context *c1 = NULL, *c2 = NULL;
    char *path1 = NULL, *path2 = NULL;
    int r = 1;

    if (args.inputs_num != 2) {
        fprintf(stderr, "ERROR: Need exactly one repository snapshot and one repository for merge command\n");
        goto finish;
    }

    if (isdirectory(args.inputs[1]) <= 0) {
        fprintf(stderr, "ERROR: %s is not a directory\n", args.inputs[1]);
        goto finish;
    }

    if (args.local_temp_flag) {
        const char *p = get_attached_filename(args.inputs[1], SYREP_TEMPDIR);
        mkdir(p, 0777);
        set_tmp_dir(p);
    }
    
    if (!(path1 = strdup(get_snapshot_filename(args.inputs[0], SYREP_SNAPSHOTFILENAME))))
        goto finish;
    
    if (!(path2 = strdup(get_snapshot_filename(args.inputs[1], SYREP_SNAPSHOTFILENAME))))
        goto finish;
    
    if (!strcmp(path1, path2)) {
        fprintf(stderr, "ERROR: merge command requires two distinct snapshots as arguments\n");
        goto finish;
    }

    if (!(c1 = db_context_open(path1, 0)))
        goto finish;
    
    if (!(c2 = db_context_open(path2, 0)))
        goto finish;

    if (db_context_origin_warn(c2))
        goto finish;
    
    if (merge(c1, c2, args.inputs[1], isdirectory(args.inputs[0]) > 0 ? args.inputs[0] : NULL) < 0)
        goto finish;

    r = 0;
    
finish:
    if (c1)
        db_context_free(c1);

    if (c2)
        db_context_free(c2);

    if (path1)
        free(path1);

    if (path2)
        free(path2);

    return r;
}

static int do_makepatch(void) {
    struct syrep_db_context *c1 = NULL, *c2 = NULL;
    char *path1 = NULL, *path2 = NULL;
    int r = 1;

    if (args.inputs_num != 2) {
        fprintf(stderr, "ERROR: Need exactly one repository and one repository snapshot for makepatch command\n");
        goto finish;
    }

    if (isdirectory(args.inputs[0]) <= 0) {
        fprintf(stderr, "ERROR: %s is not a directory\n", args.inputs[0]);
        goto finish;
    }
    
    if (args.local_temp_flag) {
        const char *p = get_attached_filename(args.inputs[0], SYREP_TEMPDIR);
        mkdir(p, 0777);
        set_tmp_dir(p);
    }
    
    if (!(path1 = strdup(get_snapshot_filename(args.inputs[0], SYREP_SNAPSHOTFILENAME))))
        goto finish;
    
    if (!(path2 = strdup(get_snapshot_filename(args.inputs[1], SYREP_SNAPSHOTFILENAME))))
        goto finish;
    
    if (!strcmp(path1, path2)) {
        fprintf(stderr, "ERROR: makepatch command requires two distinct snapshots as arguments\n");
        goto finish;
    }

    if (!(c1 = db_context_open(path1, 0)))
        goto finish;
    
    if (db_context_origin_warn(c1))
        goto finish;
    
    if (!(c2 = db_context_open(path2, 0)))
        goto finish;

    if (makepatch(c1, c2, args.inputs[0]) < 0)
        goto finish;

    if (!args.output_file_given && isatty(fileno(stdout)))
        fprintf(stderr, "Sorry, I am not going to write the patch data to a tty.\n");
    else if (db_context_save(c1, args.output_file_given ? args.output_file_arg : NULL) < 0)
        goto finish;

    r = 0;
    
finish:
    if (c1)
        db_context_free(c1);

    if (c2)
        db_context_free(c2);

    if (path1)
        free(path1);

    if (path2)
        free(path2);

    return r;
}

static int do_foreach(int (*func) (struct syrep_db_context *c), int m) {
    char *path = NULL;
    int r = 1;
    unsigned i;
    struct syrep_db_context *c = NULL;

    if (args.inputs_num < 1)
        fprintf(stderr, "WARNING: No repository or snapshot to specified!\n");
    
    for (i = 0; i < args.inputs_num; i++) {
        static char saved_cwd[PATH_MAX];

        if (args.local_temp_flag && isdirectory(args.inputs[i]) >= 1) {
            const char *p = get_attached_filename(args.inputs[i], SYREP_TEMPDIR);
            mkdir(p, 0777);
            set_tmp_dir(p);
        }
        
        if (!(path = strdup(get_snapshot_filename(args.inputs[i], SYREP_SNAPSHOTFILENAME))))
            goto finish;
        
        if (!(c = db_context_open(path, 0)))
            goto finish;

        if (args.inputs_num > 1)
            fprintf(stderr, "*** %s ***\n", path);
        
        if (m && args.output_directory_given) {
            if (!getcwd(saved_cwd, sizeof(saved_cwd))) {
                fprintf(stderr, "getcwd(): %s\n", strerror(errno));
                return -1;
            }
            
            mkdir(args.output_directory_arg, 0777);
            
            if (chdir(args.output_directory_arg) < 0) {
                fprintf(stderr, "Failed to chdir() to directory %s: %s\n", args.output_directory_arg, strerror(errno));
                return -1;
            }
        }
    
        if (func(c) < 0)
            goto finish;

        if (m && args.output_directory_given) {
            if (chdir(saved_cwd) < 0) {
                fprintf(stderr, "Failed to chdir() back to directory %s: %s\n", saved_cwd, strerror(errno));
                return -1;
            }
        }

        if (args.inputs_num > 1 && i < args.inputs_num-1)
            fprintf(stderr, "\n");
        
        db_context_free(c);
        c = NULL;
        
        free(path);
        path = NULL;
    }

    r = 0;
    
finish:
    if (c)
        db_context_free(c);

    if (path)
        free(path);

    return r;
}

static int do_update(void) {
    char *path = NULL;
    int r = 1;
    unsigned i;
    struct syrep_db_context *c = NULL;
    struct syrep_md_cache *cache = NULL;

    if (args.inputs_num < 1)
        fprintf(stderr, "WARNING: No repository to update specified!\n");

    if (args.snapshot_given && args.inputs_num != 1) {
        fprintf(stderr, "ERROR: If a snapshot file is specified only a single directory name is accepted!\n");
        goto finish;
    }
    
    for (i = 0; i < args.inputs_num; i++) {
        static char saved_cwd[PATH_MAX];
        
        if (isdirectory(args.inputs[i]) <= 0) {
            fprintf(stderr, "%s is not a directory\n", args.inputs[i]);
            return 1;
        }

        if (args.local_temp_flag) {
            const char *p = get_snapshot_filename(args.inputs[i], SYREP_TEMPDIR);
            mkdir(p, 0777);
            set_tmp_dir(p);
        }
        
        if (!(path = strdup(get_snapshot_filename(args.inputs[i], SYREP_SNAPSHOTFILENAME))))
            goto finish;
        
        if (!(c = db_context_open(path, 1)))
            goto finish;

        if (db_context_origin_warn(c))
            goto finish;

        if (db_context_fix_origin(c) < 0)
            goto finish;

        if (!args.no_cache_flag) {
            const char *p;
            
            if (args.cache_given)
                cache = md_cache_open(args.cache_arg, args.ro_cache_flag);
            else if ((p = get_attached_filename(args.inputs[i], SYREP_MDCACHEFILENAME)))
                cache = md_cache_open(p, args.ro_cache_flag);
        }

        if (!getcwd(saved_cwd, sizeof(saved_cwd))) {
            fprintf(stderr, "getcwd(): %s\n", strerror(errno));
            goto finish;
        }
        
        if (chdir(args.inputs[i]) < 0) {
            fprintf(stderr, "Failed to chdir() to directory %s: %s\n", args.inputs[i], strerror(errno));
            goto finish;
        }
        
        if (update(c, cache) < 0)
            goto finish;

        if (chdir(saved_cwd) < 0) {
            fprintf(stderr, "Failed to chdir() back to directory %s: %s\n", saved_cwd, strerror(errno));
            goto finish;
        }

        if (db_context_save(c, path) < 0)
            goto finish;

        if (!args.no_purge_flag && cache) 
            md_cache_vacuum(cache); 
        
        db_context_free(c);
        c = NULL;

        free(path);
        path = NULL;

        if (cache) {
            md_cache_close(cache);
            cache = NULL;
        }
    }

    r = 0;
        
finish:
    if (c)
        db_context_free(c);
    
    if (path)
        free(path);

    if (cache)
        md_cache_close(cache);

    return r;
}

static int do_cleanup(void) {
    int r = 1;
    unsigned i;

    if (args.inputs_num < 1)
        fprintf(stderr, "WARNING: No repository specified!\n");
    
    for (i = 0; i < args.inputs_num; i++) {

        if (isdirectory(args.inputs[i]) <= 0) {
            fprintf(stderr, "%s is not a directory\n", args.inputs[i]);
            goto finish;
        }

        if (args.inputs_num > 1)
            fprintf(stderr, "*** %s ***\n", args.inputs[i]);
        
        if (cleanup(args.inputs[i]) < 0)
            goto finish;

        if (args.inputs_num > 1 && i < args.inputs_num-1)
            fprintf(stderr, "\n");
    }

    r = 0;
    
finish:

    return r;
}

static int do_forget(void) {
    char *path = NULL;
    int r = 1;
    unsigned i;
    struct syrep_db_context *c = NULL, *target = NULL;

    if (args.inputs_num < 1)
        fprintf(stderr, "WARNING: No repository or snapshot to specified!\n");
    
    for (i = 0; i < args.inputs_num; i++) {
        if (args.local_temp_flag && isdirectory(args.inputs[i]) >= 1) {
            const char *p = get_attached_filename(args.inputs[i], SYREP_TEMPDIR);
            mkdir(p, 0777);
            set_tmp_dir(p);
        }
        
        if (!(path = strdup(get_snapshot_filename(args.inputs[i], SYREP_SNAPSHOTFILENAME))))
            goto finish;
        
        if (!(c = db_context_open(path, 0)))
            goto finish;

        if (!(target = db_context_open(NULL, 1)))
            goto finish;
        
        if (args.inputs_num > 1)
            fprintf(stderr, "*** %s ***\n", path);

        if (forget(c, target) < 0)
            goto finish;

        if (args.inputs_num > 1 && i < args.inputs_num-1)
            fprintf(stderr, "\n");
        
        db_context_free(c);

        if (db_context_save(target, path) < 0)
            goto finish;
        
        db_context_free(target);
        c = NULL;
        target = NULL;
        
        free(path);
        path = NULL;
    }

    r = 0;
    
finish:
    if (c)
        db_context_free(c);

    if (target)
        db_context_free(target);

    if (path)
        free(path);

    return r;
}

static void sigint(int s) {
    interrupted = 1;
}

static void free_args(void) {
    unsigned i;
    
    for (i = 0; i < args.inputs_num; i++)
        free(args.inputs[i]);

    free(args.inputs);
}

static int help(FILE *f, const char *argv0) {

    fprintf(f,
            "%s -- Synchronize File Repositories\n\n"

            "Usage: %s [options...] <command> [arguments...]\n\n"

            "General options:\n"
            "  -v --verbose                         Enable verbose operation\n"
            "  -T --local-temp                      Use temporary directory inside\n"
            "                                       repository\n"
            "  --ignore-origin                      Don't warn if snapshot not local in\n"
            "                                       update, merge, makepatch\n"
            "  -z --compress                        Compress snapshots or patches\n"
            "  -p --progress                        Show progress\n\n"

            "General commands:\n"
            "  -h --help                            Print help and exit\n"
            "  -V --version                         Print version and exit\n\n"

            "Specific commands:\n"
            "  --list SNAPSHOT                      List a repository snapshot\n"
            "    --show-deleted                     Show deleted entries of repository\n"
            "                                       snapshot\n"
            "    --show-by-md                       Show files by message digests\n"
            "    --show-times                       Show first and last seen times\n"
            "    --sort                             Sort chronologically\n\n"

            "  --info SNAPSHOT                      Show information about a repository or\n"
            "                                       snapshot\n\n" 

            "  --history SNAPSHOT                   Show history of a repository or snapshot\n\n"
            
            "  --dump SNAPSHOT                      Show a structure dump of a repository or\n"
            "                                       snapshot\n\n"
            
            "  --update DIRECTORY                   Update (or create) a repository snapshot\n"
            "    -SSTRING --snapshot=STRING         Use a different snapshot file than the\n"
            "                                       one contained in the repository\n"
            "    -CSTRING --cache=STRING            Use a different cache file than the one\n"
            "                                       contained in the repository\n"
            "    --no-cache                         Don't use a message digest cache\n"
            "    --no-purge                         Don't purge obsolete entries from cache\n"
            "                                       after update run\n"
            "    --ro-cache                         Use read only cache\n"
            "    --check-dev                        Store information about the device where\n"
            "                                       a file resides in the MD cache\n\n"
            
            "  --diff SNAPSHOT SNAPSHOT             Show difference between two repositories\n"
            "                                       or snapshots\n"
            "    --sizes -s                         Show sizes for files to copy (works only\n"
            "                                       with repositories)\n"
            "    --human-readable -H                Show sizes in human readable from\n"
            
            "  --merge SNAPSHOT DIRECTORY           Merge a snapshot into a repository\n"
            "                                       (perform deletes, renames only)\n"
            "  --merge PATCH DIRECTORY              Merge a patch into a repository\n"
            "  --merge DIRECTORY DIRECTORY          Merge a repository into a repository\n"
            "    -q --question                      Ask a question before each action\n"
            "    -P --prune-empty                   Prune empty directories\n"
            "    --keep-trash                       Don't empty trash\n"
            "    --check-md                         Check message digest of files prior to\n"
            "                                       deletion or replacement\n"
            "    --always-copy                      Always copy instead of hard link\n\n",
            argv0, argv0);

    fprintf(f, 
            "  --makepatch DIRECTORY SNAPSHOT       Make a patch against the specified\n"
            "                                       repository\n"
            "    -oSTRING --output-file=STRING      Write output to specified file instead\n"
            "                                       of STDOUT\n"
            "    --include-all                      Include files in patch which do exist on\n"
            "                                       the other side under a \n"
            "                                       different name\n\n"
            
            "  --extract SNAPSHOT                   Extract the contents of a snapshot or\n"
            "                                       patch\n"
            "    -DSTRING --output-directory=STRING Write output to specified directory\n\n"
            
            "  --cleanup DIRECTORY                  Remove syrep info from repository\n"
            "    -lINT --cleanup-level=INT          1 - just remove temporary data and trash\n"
            "                                           (default)\n"
            "                                       2 - remove MD cache as well\n"
            "                                       3 - remove all syrep data\n\n"
            "  --forget SNAPSHOT                    Repackage snapshot dropping outdated\n"
            "                                       information\n"
            "    --remember DAYS                    Information of how many days should be\n"
            "                                       kept? (defaults to 180)\n");
            

    return 0;
}

static int version(const char *argv0) {
    int major, minor, patch;

    db_version(&major, &minor, &patch);
    
    fprintf(stderr,
            "%s "PACKAGE_VERSION"\n"
            "Compiled with %i Bit off_t.\n"
            "Compiled with zlib %s, linked to zlib %s.\n"
            "Compiled with libdb %i.%i.%i, linked to libdb %i.%i.%i\n"
            "SVN Revision "SVN_REVISION"\n"
#ifdef USE_XATTR
            "Compiled with extended attribute support: yes\n"
#else
            "Compiled with extended attribute support: no\n"
#endif
#ifdef USE_SENDFILE
            "Using sendfile(): yes\n",
#else
            "Using sendfile(): no\n",
#endif
            argv0,
            sizeof(off_t)*8,
            ZLIB_VERSION, zlibVersion(),
            DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
            major, minor, patch );

    return 0;
}

int main(int argc, char *argv[]) {
    char *bn;

    if ((bn = strrchr(argv[0], '/')))
        bn++;
    else
        bn = argv[0];

    signal(SIGINT, sigint);
    
    cmdline_parser(argc, argv, &args);
    atexit(free_args);

    if (args.help_given)
        return help(stdout, bn);
    else if (args.version_given)
        return version(bn);
    else if (args.list_flag) 
        return do_foreach(list, 0);
    else if (args.info_flag) 
        return do_foreach(info, 0);
    else if (args.history_flag)
        return do_foreach(history, 0);
    else if (args.diff_flag)
        return do_diff();
    else if (args.update_flag)
        return do_update();
    else if (args.merge_flag)
        return do_merge();
    else if (args.dump_flag)
        return do_foreach(dump, 0);
    else if (args.extract_flag)
        return do_foreach(extract, 1);
    else if (args.makepatch_flag)
        return do_makepatch();
    else if (args.cleanup_flag)
        return do_cleanup();
    else if (args.forget_flag)
        return do_forget();

    help(stderr, bn);
    
    return 1;
}
