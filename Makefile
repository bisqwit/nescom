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

DEPDIRS =

OPTIM=-O3

CPPFLAGS += -I.

VERSION=0.1.0

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
          space.cc space.hh romaddr.hh \
          binpacker.hh binpacker.tcc \
          logfiles.hh \
          rangeset.hh rangeset.tcc range.hh \
          miscfun.hh miscfun.tcc

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin

ARCHNAME=nescom-$(VERSION)
ARCHDIR=archives/

PROGS=nescom disasm

INSTALLPROGS=nescom
INSTALL=install

all: $(PROGS)

nescom: \
		assemble.o insdata.o object.o \
		expr.o parse.o precompile.o \
		dataarea.o \
		main.o warning.o
	$(CXX) $(CXXFLAGS) -g -o $@ $^ \
		$(LDFLAGS)

disasm: disasm.o romaddr.o o65.o
	$(CXX) $(CXXFLAGS) -g -o $@ $^

clean: FORCE
	rm -f *.o $(PROGS)
distclean: clean
	rm -f *~ .depend
realclean: distclean


#../assemble: FORCE
#	$(MAKE) -C ../.. utils/assemble
#
#%.o: %.cc FORCE
#	$(MAKE) -C ../.. utils/asm/$@

include depfun.mak

.PHONY: all clean distclean realclean

FORCE: ;
