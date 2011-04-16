CC=gcc
CFLAGS=-O2 -g $(ECFLAGS)
ECFLAGS=
LD=$(CC)
LDFLAGS=$(ELDFLAGS)
LIBS=-lportmidi -lporttime -lm
ELDFLAGS=
AR=ar
ARFLAGS=rc
RANLIB=ranlib

MIDIFILE_OS=midifile.o midifilealloc.o midifstream.o

all: libmidifile.a playfile

libmidifile.a: $(MIDIFILE_OS)
	$(AR) $(ARFLAGS) libmidifile.a $(MIDIFILE_OS)
	$(RANLIB) libmidifile.a

playfile: playfile.o libmidifile.a
	$(LD) $(CFLAGS) $(LDFLAGS) $< libmidifile.a $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o libmidifile.a playfile
