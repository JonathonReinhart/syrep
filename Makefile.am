# $Id: Makefile.am 41 2003-11-12 21:42:07Z lennart $

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

EXTRA_DIST=bootstrap.sh README LICENSE
SUBDIRS=src doc man

MAINTAINERCLEANFILES = README
noinst_DATA = README

README:
	rm -f README
	$(MAKE) -C doc README
	ln -s doc/README README

homepage: all dist
	test -d $$HOME/homepage/lennart
	mkdir -p $$HOME/homepage/lennart/projects/syrep
	cp *.tar.gz $$HOME/homepage/lennart/projects/syrep
	cp doc/README.html doc/style.css man/syrep.1.xml man/xmltoman.css man/xmltoman.xsl man/xmltoman.dtd $$HOME/homepage/lennart/projects/syrep
	ln -sf $$HOME/homepage/lennart/projects/syrep/README.html $$HOME/homepage/lennart/projects/syrep/index.html

distcleancheck:
        @:

.PHONY: homepage distcleancheck
