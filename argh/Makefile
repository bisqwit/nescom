VERSION=1.0.8
ARCHDIR=archives/
ARCHNAME=libargh-$(VERSION)
ARCHFILES=argh.cc argh.hh argh.h argh-c.inc \
          Makefile.sets progdesc.php COPYING

include Makefile.sets

CPPFLAGS+=-DNEED_ARGH_H
INSTALL=install
INSTALLLIBS=libargh.so
LIBDIR=/usr/local/lib
INCDIR=/usr/local/include
DEPFUN_INSTALL=bin

all: libargh.so libargh.a

libargh.so: argh.lo
	$(CXX) -g -shared -Wl,-soname=$@ -o $@ $^
argh.lo: argh.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -fpic -o $@ -c $<

libargh.a: argh.o
	ar -rc $@ $^

include depfun.mak

install: libargh.so libargh.a argh.hh argh.h install$(DEPFUN_INSTALL)
	$(INSTALL) -s -m 0755 libargh.so $(LIBDIR)/libargh-$(VERSION).so
	$(INSTALL) -s -m 0644 libargh.a $(LIBDIR)/
	$(INSTALL) -m 0644 argh.hh argh.h $(INCDIR)/
uninstall: uninstall$(DEPFUN_INSTALL)
	rm -f $(LIBDIR)/libargh-$(VERSION).so $(LIBDIR)/libargh.a
	rm -f $(INCDIR)/argh.hh $(INCDIR)/argh.h
	
clean:
	rm -f libargh.so argh.lo argh.o libargh.a
distclean: clean
	rm -f *~ .depend
realclean: distclean

FORCE: ;
.PHONY: clean distclean realclean all FORCE install uninstall
