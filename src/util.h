#ifndef fooutilhfoo
#define fooutilhfoo

/* $Id: util.h 57 2004-07-18 18:47:55Z lennart $ */

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

#include <db.h>
#include <inttypes.h>

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

void statistics(DB *db);
char* normalize_path(char *s);
void rotdash(void);
void rotdash_hide(void);
const char* get_snapshot_filename(const char *path, const char *fn);
const char* get_attached_filename(const char *root, const char *fn);
int isdirectory(const char *path);
int copy_fd(int sfd, int dfd, off_t l);
int copy_file(const char *src, const char *dst, int c);
int move_file(const char *src, const char *dst, int c);
int copy_or_link_file(const char *src, const char *dst, int c);

/* Remove all directories between path and root if they are empty. */
int prune_empty_directories(const char *path, const char *root);

/* Same as /bin/mkdir -p in the shell */
int mkdir_p(const char *path, mode_t m);

/* Create all leading directories in path */
int makeprefixpath(const char *path, mode_t m);

int question(const char *q, const char *resp);

/* Same as /bin/rm -rf in the shell */
int rm_rf(const char *root, int rec);

ssize_t loop_read(int fd, void *d, size_t l);
ssize_t loop_write(int fd, void *d, size_t l);

int expand_file(int fd, off_t l);
off_t filesize(int fd);
off_t filesize2(const char *p);
char *snprint_off(char *s, size_t l, off_t off);

#endif
