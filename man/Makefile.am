# $Id: Makefile.am 32 2003-09-07 23:11:37Z lennart $

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

man_MANS = syrep.1

noinst_DATA = syrep.1.xml

EXTRA_DIST = $(man_MANS) syrep.1.xml.in man.css man.xsl

CLEANFILES = syrep.1.xml

syrep.1.xml: syrep.1.xml.in Makefile
	sed -e 's,@sysconfdir\@,$(sysconfdir),g' -e 's,@sbindir\@,$(sbindir),g' -e 's,@PACKAGE_URL\@,$(PACKAGE_URL),g' -e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' $< > $@

xmllint: syrep.1.xml
	xmllint $< > /dev/null

.PHONY: xmllint

if USE_XMLTOMAN

XMLTOMAN = ~/xmltoman.d/xmltoman
CLEANFILES += $(man_MANS)

syrep.1: syrep.1.xml Makefile
	$(XMLTOMAN) $< > $@

endif
