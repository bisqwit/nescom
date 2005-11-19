include Makefile.sets

# Building for Windows (opt/xmingw is for Gentoo):
#HOST=/usr/local/mingw32/bin/i586-mingw32msvc-
#HOST=/opt/xmingw/bin/i386-mingw32msvc-
#CFLAGS += -Iwinlibs
#CPPFLAGS += -Iwinlibs
#CXXFLAGS += -Iwinlibs
##LDOPTS = -L/usr/local/mingw32/lib
#LDOPTS += -L/opt/xmingw/lib
#LDFLAGS += -Lwinlibs -liconv

# Building for native:
#HOST=
#LDFLAGS += -pthread 

CXX=$(HOST)g++
CC=$(HOST)gcc
CPP=$(HOST)gcc

DEPDIRS = argh/

OPTIM=-O3

CPPFLAGS += -I. -Iargh

# For DYNAMIC argh-linking, use:
#ARGHLINK=-Largh -largh
# For STATIC argh-linking, use:
ARGHLINK=argh/libargh.a
#ARGHLINK=utils/asm/argh/libargh.a

VERSION=0.0.0

ARCHFILES=COPYING Makefile.sets progdesc.php \
          assemble.cc assemble.hh \
          tristate \
          hash.hh \
          expr.cc expr.hh \
          insdata.cc insdata.hh \
          parse.cc parse.hh \
          object.cc object.hh \
          precompile.cc precompile.hh \
          warning.cc warning.hh \
          dataarea.cc dataarea.hh \
          main.cc \
          \
          disasm.cc \
          \
          o65.cc o65.hh relocdata.hh \
          o65linker.cc o65linker.hh \
          refer.cc refer.hh msginsert.hh \
          space.cc space.hh rommap.hh \
          binpacker.hh binpacker.tcc \
          logfiles.hh \
          rangeset.hh rangeset.tcc range.hh \
          \
          argh/argh.cc argh/argh.hh argh/argh.h argh/argh-c.inc argh/docmaker.php \
          argh/Makefile argh/depfun.mak argh/Makefile.sets argh/progdesc.php \
          argh/COPYING argh/README.html

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin

ARCHNAME=nescom-$(VERSION)
ARCHDIR=archives/

PROGS=nescom

INSTALLPROGS=snescom
INSTALL=install

all: $(PROGS)

nescom: \
		assemble.o insdata.o object.o \
		expr.o parse.o precompile.o \
		dataarea.o \
		main.o warning.o \
		argh/libargh.a
	$(CXX) $(CXXFLAGS) -g -o $@ $^ \
		$(LDFLAGS) $(ARGHLINK)

disasm: disasm.o
	$(CXX) $(CXXFLAGS) -g -o $@ $^

argh/libargh.a: FORCE
	$(MAKE) -C argh libargh.a

clean: FORCE
	rm -f *.o $(PROGS) argh/*.o argh/*.lo argh/libargh.a argh/libargh.so
distclean: clean
	rm -f *~ .depend argh/*~ argh/.depend
realclean: distclean


#../assemble: FORCE
#	$(MAKE) -C ../.. utils/assemble
#
#%.o: %.cc FORCE
#	$(MAKE) -C ../.. utils/asm/$@

include depfun.mak

.PHONY: all clean distclean realclean

FORCE: ;
