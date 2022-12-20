CXXFLAGS=-O2 -g -Ibzlib

all: bzlib mtbzip2 mpibzip2

bzlib:
	wget http://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
	tar xf bzip2-1.0.8.tar.gz
	mv bzip2-1.0.8 bzlib
	rm -rf bzip2-1.0.8.tar.gz

bzlib/libbz2.a: bzlib
	cd bzlib && make libbz2.a

mtbzip2: mtbzip2.cc bzlib/libbz2.a
	g++ $(CXXFLAGS) -o mtbzip2 mtbzip2.cc bzlib/libbz2.a -lpthread

mpibzip2: mtbzip2.cc bzlib/libbz2.a
	mpicxx -DMPIBZIP2 $(CXXFLAGS) -o mpibzip2 mtbzip2.cc bzlib/libbz2.a -lpthread

MPICH=/cygdrive/c/Program\ Files/MPICH2
mpibzip2.exe: mtbzip2.cc bzlib/libbz2.a
	g++ -DMPIBZIP2 $(FLAGS) -I$(MPICH)/include -o mpibzip2.exe \
	    mtbzip2.cc bzlib/libbz2.a $(MPICH)/lib/mpi.lib

clean:
	rm -rf bzlib mtbzip2 mtbzip2.exe mpibzip2 mpibzip2.exe mpibzip2.o
