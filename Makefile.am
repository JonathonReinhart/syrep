# $Id: Makefile.am 115 2006-05-19 10:15:14Z lennart $

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

EXTRA_DIST=bootstrap.sh README LICENSE debian/changelog debian/compat debian/control debian/copyright debian/dirs debian/docs debian/rules lib/gnugetopt.h
SUBDIRS=src doc man

MAINTAINERCLEANFILES = README
noinst_DATA = README

README:
	rm -f README
	$(MAKE) -C doc README
	ln -s doc/README README

homepage: all dist
	test -d $$HOME/homepage/private
	mkdir -p $$HOME/homepage/private/projects/syrep
	cp syrep-@PACKAGE_VERSION@.tar.gz $$HOME/homepage/private/projects/syrep
	cp doc/README.html doc/style.css man/syrep.1.xml man/xmltoman.css man/xmltoman.xsl man/xmltoman.dtd $$HOME/homepage/private/projects/syrep
	ln -sf README.html $$HOME/homepage/private/projects/syrep/index.html
	rm -f $$HOME/homepage/private/projects/syrep/syrep-@PACKAGE_VERSION@.zip 
	zip -j $$HOME/homepage/private/projects/syrep/syrep-@PACKAGE_VERSION@.zip win32/*.dll win32/*.exe 

deb:
	dpkg-buildpackage -rfakeroot

.PHONY: homepage deb
