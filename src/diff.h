#ifndef foodiffhfoo
#define foodiffhfoo

/* $Id: diff.h 13 2003-08-29 01:21:11Z lennart $ */

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
#include "dbstruct.h"

/*
 * Please note that DIFF_REPLACE is never set by make_diff(). Instead
 * it is used for conflict resolution in merge.c
 */

enum { DIFF_COPY, DIFF_DELETE, DIFF_CONFLICT, DIFF_REPLACE };

struct diff_entry {
    int action;
    struct syrep_db_context *repository;
};

DB* make_diff(struct syrep_db_context *c1, struct syrep_db_context *c2);
int diff_foreach(DB *ddb, int (*cb)(DB *db, struct syrep_name *name, struct diff_entry *de, void *p), void *p);
int list_diff(struct syrep_db_context *c1, struct syrep_db_context *c2, DB *ddb);

#endif
