#ifndef foodbstructhfoo
#define foodbstructhfoo

/* $Id: dbstruct.h 58 2004-07-19 16:04:48Z lennart $ */

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

#include <limits.h>
#include <inttypes.h>

#include <db.h>

#include "syrep.h"

struct syrep_md {
    uint8_t digest[SYREP_DIGESTLENGTH]; 
};

struct syrep_nrecno {                        /* name record number */
    db_recno_t recno;
};

struct syrep_id {
    struct syrep_md md;
    struct syrep_nrecno nrecno;
};

struct syrep_meta {
    uint32_t first_seen;
    uint32_t last_seen;
};

struct syrep_timestamp {
    uint32_t t;
};

struct syrep_version {
    uint32_t v;
};

struct syrep_nhash {                     /* name hash */
    uint32_t hash;
};

struct syrep_name {
    char path[PATH_MAX];
};


/* Table layout:
 *
 * syrep_id :: syrep_meta           => id_meta 
 * syrep_md :: syrep_nrecno         => md_nrecno                           (DUP)
 * syrep_nrecno :: syrep_md         => nrecno_md                           (DUP)
 * 
 * syrep_nrecno :: syrep_md         => nrecno_lastmd
 * syrep_md :: syrep_nrecno         => md_lastnrecno
 *
 * syrep_version :: syrep_timestamp => version_timestamp
 *
 * nhash :: nrecno                  => nhash_nrecno                        (DUP)
 * nrecno :: name                   => nrecno_name
 */

#endif
