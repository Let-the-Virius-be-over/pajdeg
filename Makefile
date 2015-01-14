CFLAGS  = -I include
CC      = gcc
SRCDIR	= src

all:	pajdeg

pajdeg:	$(SRCDIR) libpajdeg.a
	cd $(SRCDIR) && make 
	mv $(SRCDIR)/libpajdeg.a .

debug:	$(SRCDIR)
	cd $(SRCDIR) && make debug
	mv $(SRCDIR)/libpajdegD.a .

clean:
	rm libpajdeg*.a
	cd $(SRCDIR) && make clean
