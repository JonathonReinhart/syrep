#ifndef foomd5utilhfoo
#define foomd5utilhfoo

/* $Id: md5util.h 12 2003-08-28 22:44:04Z lennart $ */

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

void fhex(const unsigned char *bin, int len, char *txt);
#define fhex_md5(bin,txt) fhex((bin),16,(txt))

int fdmd5(int fd, size_t l, char *md);

#endif
