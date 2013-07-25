CFLAGS  = -I include
CC      = gcc
SRCDIR	= src

all:	pajdeg

pajdeg:	$(SRCDIR)
	cd $(SRCDIR) && make 
	cp $(SRCDIR)/libpajdeg.a .

debug:	$(SRCDIR)
	cd $(SRCDIR) && make debug
	cp $(SRCDIR)/libpajdegD.a .

clean:
	rm libpajdeg*.a
	cd $(SRCDIR) && make clean
