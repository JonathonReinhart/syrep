/* $Id: info.c 19 2003-08-31 20:46:56Z lennart $ */

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

#include <stdio.h>
#include <assert.h>

#include "info.h"
#include "util.h"

int info(struct syrep_db_context *c) {
    assert(c);
    fprintf(stderr, "Origin: %s\n", c->origin);
    fprintf(stderr, "Timestamp: %u\n", c->timestamp);
    fprintf(stderr, "Version: %u\n", c->version);
    fprintf(stderr, "Database nrecno_meta: ");
    statistics(c->db_id_meta);
    fprintf(stderr, "Database md_nrecno: ");
    statistics(c->db_md_nrecno);
    fprintf(stderr, "Database nrecno_md: ");
    statistics(c->db_nrecno_md);
    fprintf(stderr, "Database nrecno_lastmd: ");
    statistics(c->db_nrecno_lastmd);
    fprintf(stderr, "Database md_lastnrecno: ");
    statistics(c->db_md_lastnrecno);
    fprintf(stderr, "Database version_timestamp: ");
    statistics(c->db_version_timestamp);
    fprintf(stderr, "Database nrecno_name: ");
    statistics(c->db_nrecno_name);
    fprintf(stderr, "Database nhash_nrecno: ");
    statistics(c->db_nhash_nrecno);
    return 0;
}
