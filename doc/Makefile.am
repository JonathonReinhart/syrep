# $Id: Makefile.am 7 2003-08-04 23:05:55Z lennart $

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

noinst_DATA = README.html README
EXTRA_DIST = $(noinst_DATA) style.css README.html.in todo.txt package.txt usage-intro.txt

MAINTAINERCLEANFILES = README README.html
CLEANFILES =

if USE_LYNX
README: README.html
	lynx --dump $^ | sed 's,file://localhost/.*/doc/README.html,README,' > $@

CLEANFILES += README
endif

tidy: README.html
	tidy -e < README.html

.PHONY: tidy
