#ifndef foopackagehfoo
#define foopackagehfoo

/* $Id: package.h 103 2006-04-22 10:57:59Z lennart $ */

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

#include <inttypes.h>

#define PACKAGE_FILEID (*((const uint32_t*) "SREP"))
#define PACKAGE_FILEIDCOMPRESSED (*((const uint32_t*) "CREP"))

#define PACKAGE_ITEMNAMELEN 32

struct package;

struct package* package_open(const char *fn, int force);
void package_remove(struct package *p);
int package_save(struct package *p, const char *fn);
int package_get_item(struct package* p, const char *name, int c, char const ** fn);
int package_add_file(struct package *p, const char *name, const char *fn);
int package_foreach(struct package *p, int (*cb) (struct package *p, const char *name, const char *path, void *u), void *u);

#endif
