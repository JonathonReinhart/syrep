#ifndef fooupdatehfoo
#define fooupdatehfoo

/* $Id: update.h 58 2004-07-19 16:04:48Z lennart $ */

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

#include "context.h"
#include "cache.h"
#include "dbstruct.h"

int update(struct syrep_db_context *c, struct syrep_md_cache *cache);

/* This is exported solely for usage in forget.c */
int write_entry(struct syrep_db_context *c, const struct syrep_name *name, const struct syrep_md *md, const struct syrep_meta *meta);

#endif
