#######################################
# Makefile for plp:
# NB. Did you remember to look at defaults.h ?
#
# Commands:
#	make - to compile
# 	make test - to compile and run a little test
#	make install - to put executables in /usr/local/bin
#	make clean - to clean the distribution
#	make killncp - to kill the ncp daemon
#######################################

#######################################
# The following are site specific, and you should scan through them
#######################################

# Compiler & flags
INCLUDE=-I../utils
CFLAGS= -Wall -g -O2
CC=g++ $(CFLAGS) $(INCLUDE)

# Linker and flags
LIBDIR=-L../utils
# Solaris users uncomment this line instead of the next one
#LIBS=-lutils -lsocket -lnsl
LIBS=-lutils
LD=g++ $(LIBDIR)

# Archiver
AR=ar -cr

#######################################
# You shouldn't need to look any further..
#######################################

export CC
export AR
export LD
export LIBS

all: bin/ncp bin/rfsv

bin/ncp: $(wildcard ncp/*.cc) ncp utils/libutils.a
	cd ncp; $(MAKE)

bin/rfsv: $(wildcard rfsv/*.cc) utils/libutils.a
	cd rfsv; $(MAKE)

utils/libutils.a:
	cd utils; $(MAKE)

install:
	cp bin/rfsv /usr/local/bin/rfsv
	cp bin/ncp /usr/local/bin/ncp

clean:
	cd utils; $(MAKE) clean
	cd ncp; $(MAKE) clean
	cd rfsv; $(MAKE) clean
	rm -f ncp.log *~ core

test: bin/ncp bin/rfsv
	PATH=`pwd`/bin:$${PATH}; export PATH;\
		rfsv dir "C:/document/"
	@echo
	rm -f ptest
	echo "	** This text file was transferred succesfully to and from the psion" > ptest
	@echo
	PATH=`pwd`/bin:$${PATH}; export PATH;\
		 rfsv write ptest "C:/ptest"
	@echo
	rm -f ptest
	@echo
	PATH=`pwd`/bin:$${PATH}; export PATH;\
		rfsv read "C:/ptest" ptest
	@echo
	PATH=`pwd`/bin:$${PATH}; export PATH;\
		rfsv del "C:/ptest"
	@echo
	cat ptest
	rm -f ptest

killncp:
	ps -aux | grep ncp/ncp | grep -v grep | awk "BEGIN { while (getline) { print \$$2}}" | xargs -r kill

