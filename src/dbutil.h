#ifndef foodbutilhfoo
#define foodbutilhfoo

/* $Id: dbutil.h 19 2003-08-31 20:46:56Z lennart $ */

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

#include "dbstruct.h"
#include "context.h"

int get_meta_by_nrecno_md(struct syrep_db_context *c, const struct syrep_nrecno*nrecno, const struct syrep_md *md, struct syrep_meta *meta);
int get_last_md_by_nrecno(struct syrep_db_context *c, const struct syrep_nrecno *nrecno, struct syrep_md *md);
int get_current_md_by_nrecno(struct syrep_db_context *c, const struct syrep_nrecno *nrecno, struct syrep_md *md);
int get_current_nrecno_by_md(struct syrep_db_context *c, const struct syrep_md *md, struct syrep_nrecno *nrecno);
uint32_t get_version_timestamp(struct syrep_db_context *c, uint32_t v);

int get_nrecno_by_name(struct syrep_db_context *c, const struct syrep_name *name, struct syrep_nrecno *nrecno, int create);
int get_name_by_nrecno(struct syrep_db_context *c, const struct syrep_nrecno *nrecno, struct syrep_name *name);

int new_name(struct syrep_db_context *c, const struct syrep_name *name, struct syrep_nrecno *rnrecno);
    
#endif
