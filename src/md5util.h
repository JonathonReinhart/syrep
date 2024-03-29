#ifndef foomd5utilhfoo
#define foomd5utilhfoo

/* $Id: md5util.h 76 2005-06-05 20:14:45Z lennart $ */

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

#include <sys/types.h>
#include <inttypes.h>

void fhex(const uint8_t *bin, size_t len, char *txt);
#define fhex_md5(bin,txt) fhex((bin),16,(txt))

int fdmd5(int fd, off_t l, uint8_t md[]);

int fmd5(const char *fn, uint8_t md[]);

#endif
