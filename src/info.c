/* $Id: info.c 43 2003-11-30 14:27:42Z lennart $ */

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
#include <time.h>

#include "info.h"
#include "util.h"

int info(struct syrep_db_context *c) {
    time_t t = (time_t) c->timestamp;
    assert(c);
    fprintf(stdout, "Origin: %s\n", c->origin);
    fprintf(stdout, "Timestamp: %u; %s", c->timestamp, asctime(localtime(&t)));
    fprintf(stdout, "Version: %u\n", c->version);
    fprintf(stdout, "Database nrecno_meta: ");
    statistics(c->db_id_meta);
    fprintf(stdout, "Database md_nrecno: ");
    statistics(c->db_md_nrecno);
    fprintf(stdout, "Database nrecno_md: ");
    statistics(c->db_nrecno_md);
    fprintf(stdout, "Database nrecno_lastmd: ");
    statistics(c->db_nrecno_lastmd);
    fprintf(stdout, "Database md_lastnrecno: ");
    statistics(c->db_md_lastnrecno);
    fprintf(stdout, "Database version_timestamp: ");
    statistics(c->db_version_timestamp);
    fprintf(stdout, "Database nrecno_name: ");
    statistics(c->db_nrecno_name);
    fprintf(stdout, "Database nhash_nrecno: ");
    statistics(c->db_nhash_nrecno);
    return 0;
}
