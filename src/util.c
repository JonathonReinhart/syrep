/* $Id: util.c 83 2005-06-20 21:08:13Z lennart $ */

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
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>

#ifdef USE_SENDFILE
#include <sys/sendfile.h>
#endif

#include "util.h"
#include "syrep.h"

static int stderr_tty = -1;

void statistics(DB *db) {
    DB_BTREE_STAT *statp;
    int ret;

    assert(db);
    
     if ((ret = db->stat(db, NULL, &statp, /* DB_FAST_STAT*/ 0)) != 0) {
        db->err(db, ret, "DB->stat");
        return;
    }
    
    printf("Database contains %lu records\n", (long unsigned) statp->bt_ndata);
    free(statp);
}

char* normalize_path(char *s) {
    char *l, *p, *d;

    /* deletes /./ and // */

    if (*s == '/')
        l = p = d = s+1;
    else
        l = p = d = s;
    
    for (; *p; p++) {
        if (*p == '/') {

            if (l-p == 0) {
                l++;
                continue;
            }

            if (p-l == 1 && *l == '.') {
                l += 2;
                continue;
            }

            while (l <= p)
                *(d++) = *(l++);
        }
    }

    while (l <= p)
        *(d++) = *(l++);

    return s;
}

void rotdash(void) {
    static const char dashes[] = /* ".oOo"; */ "|/-\\";
    static const char *d = dashes;

    if (!args.progress_flag)
        return;
    
    if (stderr_tty < 0)
        stderr_tty = isatty(fileno(stderr));
    
    if (stderr_tty) {
        fprintf(stderr, "%c\b", *d);
        
        d++;
        if (!*d)
            d = dashes;
    }
}

void rotdash_hide(void) {
    if (!args.progress_flag)
        return;

    if (stderr_tty < 0)
        stderr_tty = isatty(fileno(stderr));

    if (stderr_tty)
        fputs(" \b", stderr);
}

const char* get_attached_filename(const char *root, const char *fn) {
    static char npath[PATH_MAX];
    snprintf(npath, sizeof(npath), "%s/.syrep/%s", root, fn);
    makeprefixpath(npath, 0777);
    return npath;
}

const char* get_snapshot_filename(const char *root, const char *fn) {
    struct stat st;

    if (stat(root, &st) < 0) {
        if (errno == ENOENT)
            return root;
        
        fprintf(stderr, "stat(%s) failed: %s\n", root, strerror(errno));
        return NULL;
    }

    if (S_ISREG(st.st_mode))
        return root;

    if (S_ISDIR(st.st_mode)) 
        return get_attached_filename(root, fn);

    fprintf(stderr, "<%s> is not a valid syrep snapshot\n", root);
    return NULL;
}

int isdirectory(const char *path) {
    struct stat st;

    if (stat(path, &st) < 0)
        return -1;

    return !!S_ISDIR(st.st_mode);
}


#ifdef USE_SENDFILE    

#define MAX_SENDFILE_SIZE (1024L*1024L*1024L) /* A gigabyte */

static int copy_fd_sendfile(int sfd, int dfd, off_t l) {
    off_t sfo, o;
    int first = 1;

    if ((sfo = lseek(sfd, 0, SEEK_CUR)) == (off_t) -1)
        o = 0;
    else
        o = sfo;

    while (l > 0) {
        size_t m = MIN(l, MAX_SENDFILE_SIZE);
        ssize_t r;
        
        if ((r = sendfile(dfd, sfd, &o, m)) <= 0) {
            assert(first);
            return -1;
        }

        l -= r;

        if (sfo != (off_t) -1)
            sfo += r;

        first = 0;
    }

    if (sfo != (off_t) -1)
        lseek(sfd, sfo, SEEK_SET);

    return 0;
}

#endif
    
off_t filesize(int fd) {
    struct stat st;

    if (fstat(fd, &st) < 0) {
/*        fprintf(stderr, "stat(): %s\n", strerror(errno));*/
        return (off_t) -1;
    }

    return st.st_size;
}

int expand_file(int fd, off_t l) {
    off_t s;

    if ((s = filesize(fd)) == (off_t) -1)
        return -1;
    
    if (l > s)
        if (ftruncate(fd, l) < 0) {
            fprintf(stderr, "ftruncate(): %s\n", strerror(errno));
            return -1;
        }

    return 0;
}

#define MMAPSIZE (20*1024*1024) /* 20MB */
#define BUFSIZE (64*1024)

int copy_fd(int sfd, int dfd, off_t l) {
    off_t sfo = 0, dfo = 0, msfo = 0, mdfo = 0;
    size_t m = 0, sm = 0, dm = 0;
    uint8_t *sp, *dp;
    static size_t psize = 0;

    if (l == 0)
        return 0;
    
#ifdef USE_SENDFILE
    if (copy_fd_sendfile(sfd, dfd, l) >= 0)
        return 0;

    if (errno == ENOSPC)
        return -1;
#endif

    //fprintf(stderr, "copy_fd(%u)\n", l);
    
    if (psize == 0)
        psize = (off_t) sysconf(_SC_PAGESIZE);

    assert(psize > 0);
    
    sp = dp = MAP_FAILED;

    if (l > BUFSIZE) {

        m = (size_t) MIN(l, MMAPSIZE);

        if ((sfo = lseek(sfd, 0, SEEK_CUR)) != (off_t) -1) {
            off_t s;
            
            if ((s = filesize(sfd)) < 0)
                return -1;

            if (sfo+l > s) {
                fprintf(stderr, "File too short\n");
                return -1;
            }

            msfo = (off_t) (sfo/psize)*psize;
            sm = m+(sfo-msfo);
            if ((sp = mmap(NULL, sm, PROT_READ, MAP_SHARED, sfd, msfo)) != MAP_FAILED)
                madvise(sp, sm, MADV_SEQUENTIAL);
        }
        
        if ((dfo = lseek(dfd, 0, SEEK_CUR)) != (off_t) -1) {
            if (expand_file(dfd, dfo+l) < 0)
                return -1;

            mdfo = (off_t) (dfo/psize)*psize;
            dm = m+(dfo-mdfo);
            if ((dp = mmap(NULL, dm, PROT_READ|PROT_WRITE, MAP_SHARED, dfd, mdfo)) != MAP_FAILED)
                madvise(dp, dm, MADV_SEQUENTIAL);
        }
    }

    if (sp == MAP_FAILED && dp == MAP_FAILED) { /* copy fd to fd */
        void *buf = NULL;

        if (!(buf = malloc(BUFSIZE))) {
            fprintf(stderr, "malloc(): %s\n", strerror(errno));
            return -1;
        }

        while (l > 0) {
            off_t n;
            size_t m = MIN(l, BUFSIZE);
            
            if ((n = loop_read(sfd, buf, m)) != m) {
                
                if (n < 0)
                    fprintf(stderr, "read(): %s\n", strerror(errno));
                else
                    fprintf(stderr, "Short read\n");

                free(buf);
                return -1;
            }

            if ((n = loop_write(dfd, buf, m)) != m) {

                if (n < 0)
                    fprintf(stderr, "write(): %s\n", strerror(errno));
                else
                    fprintf(stderr, "Short write\n");

                free(buf);
                return -1;
            }

            l -= m;
        }

        free(buf);
        return 0;
    } else if (sp == MAP_FAILED) { /* copy fd to mmap */

        for (;;) {
            ssize_t n;

            n = loop_read(sfd, dp+(dfo-mdfo), m);
	    msync(dp, dm, MS_SYNC);
            munmap(dp, dm);

            if (n != (ssize_t) m) {

                if (n < 0)
                    fprintf(stderr, "read(): %s\n", strerror(errno));
                else
                    fprintf(stderr, "Short read\n");

                return -1;
            }
            

            l -= m;
            dfo += m;

            if (l <= 0) {
                if (lseek(dfd, dfo, SEEK_SET) == (off_t) -1)
                    return -1;
                
                return 0;
            }

            m = (size_t) (MIN(l, MMAPSIZE));
            
            mdfo = (off_t) (dfo/psize)*psize;
            dm = m+(dfo-mdfo);
            if ((dp = mmap(NULL, dm, PROT_READ|PROT_WRITE, MAP_SHARED, dfd, mdfo)) == MAP_FAILED) {
                fprintf(stderr, "mmap(): %s\n", strerror(errno));
                return -1;
            }
            madvise(dp, dm, MADV_SEQUENTIAL);
        }

    } else if (dp == MAP_FAILED) { /* copy mmap to fd */

        for (;;) {
            ssize_t n;

            n = loop_write(dfd, sp+(sfo-msfo), m);
            munmap(sp, sm);

            if (n != (ssize_t) m) {

                if (n < 0)
                    fprintf(stderr, "write(): %s\n", strerror(errno));
                else
                    fprintf(stderr, "Short write\n");

                return -1;
            }

            l -= m;
            sfo += m;

            if (l <= 0) {
                if (lseek(sfd, sfo, SEEK_SET) == (off_t) -1)
                    return -1;
                
                return 0;
            }

            m = (size_t) (MIN(l, MMAPSIZE));
            
            msfo = (off_t) (sfo/psize)*psize;
            sm = m+(sfo-msfo);
            if ((sp = mmap(NULL, sm, PROT_READ, MAP_SHARED, sfd, msfo)) == MAP_FAILED) {
                fprintf(stderr, "mmap(): %s\n", strerror(errno));
                return -1;
            }
            madvise(sp, sm, MADV_SEQUENTIAL);
        }
        
    } else {  /* copy mmap to mmap */

        assert(sp != MAP_FAILED && dp != MAP_FAILED);

        for (;;) {

            memcpy(dp+(dfo-mdfo), sp+(sfo-msfo), m);

            munmap(sp, sm);
	    msync(dp, dm, MS_SYNC);
            munmap(dp, dm);

            l -= m;
            sfo += m;
            dfo += m;
            
            if (l <= 0) {

                if (lseek(sfd, sfo, SEEK_SET) == (off_t) -1)
                    return -1;

                if (lseek(dfd, dfo, SEEK_SET) == (off_t) -1)
                    return -1;

                return 0;
            }

            m = (size_t) (MIN(l, MMAPSIZE));

            msfo = (off_t) (sfo/psize)*psize;
            sm = m+(sfo-msfo);
            if ((sp = mmap(NULL, sm, PROT_READ, MAP_SHARED, sfd, msfo)) == MAP_FAILED) {
                fprintf(stderr, "mmap(): %s\n", strerror(errno));
                return -1;
            }
            mdfo = (off_t) (dfo/psize)*psize;
            dm = m+(dfo-mdfo);
            if ((dp = mmap(NULL, dm, PROT_READ|PROT_WRITE, MAP_SHARED, dfd, mdfo)) == MAP_FAILED) {
                munmap(sp, sm);
                fprintf(stderr, "mmap(): %s\n", strerror(errno));
                return -1;
            }

            madvise(sp, sm, MADV_SEQUENTIAL);
            madvise(dp, dm, MADV_SEQUENTIAL);
        }
    }
}

int copy_file(const char *src, const char *dst, int c) {
    int sfd = -1, dfd = -1, r = -1;
    off_t size;
    
    if ((sfd = open(src, O_RDONLY)) < 0) {
        fprintf(stderr, "open(\"%s\", O_RDONLY): %s\n", src, strerror(errno));
        goto finish;
    }

    if ((dfd = open(dst, O_RDWR|O_TRUNC|O_CREAT|( c ? 0 : O_EXCL), 0666)) < 0) {
        fprintf(stderr, "open(\"%s\", O_RDWR|O_TRUNC|O_CREAT%s): %s\n", dst, c ? "" : "|O_EXCL", strerror(errno));
        goto finish;
    }

    if ((size = filesize(sfd)) < 0)
        goto finish;
    
    if (copy_fd(sfd, dfd, size) < 0)
        goto finish;

    r = 0;

finish:
    
    if (sfd >= 0)
        close(sfd);

    if (dfd >= 0)
        close(dfd);

    return r;
}

int copy_or_link_file(const char *src, const char *dst, int c) {

    if (c)
        unlink(dst);
    
    if (link(src, dst) < 0) {

        if (errno == EXDEV || errno == EPERM)
            return copy_file(src, dst, c);

/*         fprintf(stderr, "link(%s, %s): %s\n", src, dst, strerror(errno)); */
        return -1;
    }

    return 0;
}

int move_file(const char *src, const char *dst, int c) {
    int r;

    if ((r = copy_or_link_file(src, dst, c)) < 0)
        return -1;

    if (unlink(src) < 0) {
        fprintf(stderr, "unlink(%s): %s\n", src, strerror(errno));
        unlink(dst);
        return -1;
    }
        
    return 0;
}

int prune_empty_directories(const char *path, const char *root) {
    char rroot[PATH_MAX],
         rpath[PATH_MAX];

    strncpy(rroot, root, PATH_MAX-1);
    rroot[PATH_MAX-1] = 0;
    normalize_path(rroot);

    strncpy(rpath, path, PATH_MAX-1);
    rpath[PATH_MAX-1] = 0;
    normalize_path(rpath);

    for (;;) {
        char *e;

        if (!rpath[0] || !strcmp(rpath, "/") || !strcmp(rpath, rroot))
            break;

        if (rmdir(rpath) < 0) {

            if (errno == ENOTEMPTY || errno == EBUSY)
                break;

            if (errno != ENOENT) {
                fprintf(stderr, "rmdir(\"%s\"): %s\n", rpath, strerror(errno));
                return -1;
            }
        }

        if (!(e = strrchr(rpath, '/')))
            break;

        *e = 0;
    }

    return 0;
}

int mkdir_p(const char *path, mode_t m) {
    char tmp[PATH_MAX];
    char *e, *b;
    int quit = 0;

    strncpy(tmp, path, PATH_MAX-1);
    tmp[PATH_MAX-1] = 0;

    normalize_path(tmp);

    if (!tmp[0])
        return 0;

    for (b = tmp, quit = 0; !quit;) {
        
        if (!(e = strchr(b, '/'))) {
            e = strchr(b, 0);
            quit = 1;
        }

        if (e != b) {
            *e = 0;
            
            if (mkdir(tmp, m) < 0) {
                if (errno != EEXIST) {
                    fprintf(stderr, "mkdir(\"%s\"): %s\n", tmp, strerror(errno));
                    return -1;
                }
            }
            *e = '/';
        }
        
        b = e+1;
    }

    return 0;
}

/* Create all leading directories in path */
int makeprefixpath(const char *path, mode_t m) {
    char tmp[PATH_MAX], *e;

    strncpy(tmp, path, PATH_MAX-1);
    tmp[PATH_MAX-1] = 0;

    normalize_path(tmp);

    if (!(e = strrchr(tmp, '/')))
        return 0;

    *e = 0;

    return mkdir_p(tmp, m);
}


int question(const char *text, const char *replies) {
    int r = 0;
    FILE *f;
    
    assert(text && replies && *replies);

    if (!(f = fopen("/dev/tty", "r")))
        f = stdin;

    for (;;) {
        const char *q;
        char reply[256];

        if (interrupted) {
            fprintf(stderr, "Canceled.\n");
            goto finish;
        }
    
        fprintf(stderr, "\r%s [", text);
        
        for (q = replies; *q; q++) {
            fputc(q == replies ? toupper(*q) : tolower(*q), stderr);

            if (*(q+1))
                fputc('|', stderr);
        }
        
        fprintf(stderr, "] ");
        
        if (!fgets(reply, sizeof(reply), f))
            goto finish;

        if (reply[0] == '\r' || reply[0] == '\n')
            reply[0] = *replies;
        
        reply[0] = tolower(reply[0]);
        
        for (q = replies; *q; q++)

            if (tolower(*q) == reply[0]) {
                r = *q;
                goto finish;
            }
    }

finish:
    
    fputc('\r', stderr);
    
    if (f != stdin)
        fclose(f);

    return r;
}


int rm_rf(const char *root, int rec) {
    DIR *dir = NULL;
    int r = -1;
    struct dirent *de; 

    if (!(dir = opendir(root))) {

        if (errno == ENOENT) {
            r = 0;
            goto finish;
        }
        
        fprintf(stderr, "opendir(\"%s\"): %s", root, strerror(errno));
        goto finish;
    }

    while ((de = readdir(dir))) {
        char path[PATH_MAX];
        struct stat st;

        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        
        snprintf(path, sizeof(path), "%s/%s", root, de->d_name);

        if (lstat(path, &st) < 0) {
            fprintf(stderr, "stat(\"%s\"): %s\n", path, strerror(errno));
            goto finish;
        }

        if (S_ISDIR(st.st_mode)) {
            if (rec) {
                if (rm_rf(path, rec) < 0)
                    goto finish;
            } else {
                if (rmdir(path) < 0) {
                    fprintf(stderr, "rmdir(\"%s\"): %s\n", path, strerror(errno));
                    goto finish;
                }
            }

        } else {

            if (unlink(path) < 0) {
                fprintf(stderr, "unlink(\"%s\"): %s\n", path, strerror(errno));
                goto finish;
            }
        }
    }

    if (rmdir(root) < 0) {
        fprintf(stderr, "rmdir(\"%s\"): %s\n", root, strerror(errno));
        goto finish;
    }
    
    r = 0;

finish:
    if (dir)
        closedir(dir);
    
    return r;
}

ssize_t loop_read(int fd, void *d, size_t l) {
    uint8_t *p = d;
    
    while (l > 0) {
        ssize_t r;
        
        if ((r = read(fd, p, l)) <= 0)
            return p-(uint8_t*) d > 0 ? p- (uint8_t*)d : r;

        p += r;
        l -= r;
    }

    return p-(uint8_t*)d;
}

ssize_t loop_write(int fd, const void *d, size_t l) {
    const uint8_t *p = d;
    
    while (l > 0) {
        ssize_t r;
        
        if ((r = write(fd, p, l)) <= 0)
            return p-(uint8_t*)d > 0 ? p-(uint8_t*) d : r;

        p += r;
        l -= r;
    }

    return p-(uint8_t*) d;
}

char *snprint_off(char *s, size_t l, off_t off) {
    assert(s && l);
    
    if (args.human_readable_flag && off >= 1024*1024*1024)
        snprintf(s, l, "%0.1f GB", (double) off/1024/1024/1024);
    else if (args.human_readable_flag && off >= 1024*1024)
        snprintf(s, l, "%0.1f MB", (double) off/1024/1024);
    else if (args.human_readable_flag && off >= 1024)
        snprintf(s, l, "%0.1f KB", (double) off/1024);
    else
        snprintf(s, l, "%llu", (uint64_t) off);

    return s;
}

off_t filesize2(const char *p) {
   struct stat st;

    if (stat(p, &st) < 0)
        return (off_t) -1;

    return st.st_size;
    
}

#define TMPDIR_ENV "TMPDIR"

void set_tmp_dir(const char *s) {
#ifdef HAVE_SETENV
    setenv(TMPDIR_ENV, s, 1);
#else
    char *tmp = malloc(strlen(s) + sizeof(TMPDIR_ENV) + 1);
    assert(tmp);
    sprintf(tmp, "%s=%s", TMPDIR_ENV, s);
    putenv(tmp);
#endif
}
