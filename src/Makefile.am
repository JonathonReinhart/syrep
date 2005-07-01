# $Id: Makefile.am 88 2005-07-01 22:31:34Z lennart $

# This file is part of syrep.
#
# syrep is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# syrep is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with syrep; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.

AM_CPPFLAGS=-I$(top_srcdir)/lib

bin_PROGRAMS = syrep

noinst_PROGRAMS = rm-rf-test copy-fd-test

syrep_SOURCES = cache.c cache.h \
				update.c update.h \
				util.c util.h \
				syrep.c syrep.h \
				md5.c md5.h \
				md5util.c md5util.h \
				context.c context.h \
				package.c package.h \
				dbutil.c dbutil.h \
				info.c info.h \
				history.c history.h \
				dump.c dump.h \
				list.c list.h \
				diff.c diff.h \
				merge.c merge.h \
				extract.c extract.h \
				makepatch.c makepatch.h \
				cleanup.c cleanup.h \
				cmdline.c cmdline.h \
				svn-revision.h \
				dbstruct.h \
				forget.c forget.h 
syrep_LDADD=@LIBOBJS@

rm_rf_test_SOURCES = util.c rm-rf-test.c util.h cmdline.c cmdline.h
copy_fd_test_SOURCES = util.c copy-fd-test.c util.h cmdline.c cmdline.h

EXTRA_DIST = syrep.ggo getopt.c getopt1.c mkdtemp.c
MAINTAINERCLEANFILES =
BUILT_SOURCES = 
CLEANFILES = 

if USE_GENGETOPT 
CLEANFILES += cmdline.c cmdline.h
BUILT_SOURCES += cmdline.c cmdline.h
endif

if USE_SUBVERSION
MAINTAINERCLEANFILES += svn-revision.h
BUILT_SOURCES += svn-revision.h 
endif

syrep.$(OBJEXT): svn-revision.h cmdline.h
update.$(OBJEXT): cmdline.h

if USE_GENGETOPT

cmdline.c cmdline.h: syrep.ggo Makefile
	gengetopt --unamed-opts --no-handle-help --no-handle-version < $<

endif

if USE_SUBVERSION

svn-revision.h: Makefile
	if test -d "$(top_srcdir)/.svn" ; then \
		if REV=`svn info "$(top_srcdir)" | grep ^Revision | cut -f2 -d" "` 2> /dev/null ; then \
			echo -e "#ifndef foosvnrevisionhfoo\n#define foosvnrevisionhfoo\n#define SVN_REVISION \"$$REV\"\n#endif" > $@ ; \
	    fi \
	fi

svnkeywords:
	rm -f svn-revision.h cmdline.[ch]
	svn propset svn:keywords Id *.c *.h Makefile.am

.PHONY: svnkeywords

endif


