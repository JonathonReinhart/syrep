#ifndef foomdcachehfoo
#define foomdcachehfoo

/* $Id: cache.h 30 2003-09-04 22:06:56Z lennart $ */

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

struct syrep_md_cache;

struct syrep_md_cache* md_cache_open(const char* fn, int ro);
void md_cache_close(struct syrep_md_cache *c);
int md_cache_get(struct syrep_md_cache *c, const char *path, uint8_t digest[16]);
int md_cache_vacuum(struct syrep_md_cache *c);

#endif
