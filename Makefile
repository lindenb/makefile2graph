prefix = /usr/local
bindir = $(prefix)/bin
sharedir = $(prefix)/share
docdir = $(sharedir)/doc
pkgdocdir = $(sharedir)/makefile2graph
mandir = $(sharedir)/man
man1dir = $(mandir)/man1

bin_PROGRAMS = make2graph
bin_SCRIPTS = makefile2graph
pkgdoc_DATA = LICENSE README.md screenshot.png
man1_MANS = make2graph.1 makefile2graph.1

CFLAGS ?= -O3 -Wall

.PHONY: all clean install test
.DELETE_ON_ERROR:

all: $(bin_PROGRAMS)

clean:
	rm -f $(bin_PROGRAMS)

install:
	install -d $(DESTDIR)$(bindir) $(DESTDIR)$(pkgdocdir) $(DESTDIR)$(man1dir)
	install $(bin_PROGRAMS) $(bin_SCRIPTS) $(DESTDIR)$(bindir)
	install $(pkgdoc_DATA) $(DESTDIR)$(pkgdocdir)
	install $(man1_MANS) $(DESTDIR)$(man1dir)

test: all
	$(MAKE) -Bnd | ./make2graph | dot
	$(MAKE) -Bnd | ./make2graph --format x
	$(MAKE) -Bnd | ./make2graph --format l
	$(MAKE) -Bnd | ./make2graph --format e
	$(MAKE) -Bnd | ./make2graph --root
	PATH=.:$(PATH) ./makefile2graph
	PATH=.:$(PATH) ./makefile2graph -B

