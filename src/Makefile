CFLAGS  = -I include -c -Wall
CC      = gcc
SRC	= PDBTree.c PDEnv.c PDOperator.c PDPipe.c PDReference.c PDStack.c PDStaticHash.c PDObject.c PDParser.c PDPortableDocumentFormatState.c PDScanner.c PDState.c PDTask.c PDTwinStream.c
HDR	= PDBTree.h PDDefines.h PDEnv.h PDInternal.h PDObject.h PDOperator.h PDPDFPrivate.h PDParser.h PDPipe.h PDPortableDocumentFormatState.h PDReference.h PDScanner.h PDStack.h PDState.h PDStaticHash.h PDTask.h PDTwinStream.h Pajdeg.h

all:	pajdeg

pajdeg:	libpajdeg.a

debug:	libpajdegD.a

libpajdegD.a: $(SRC) $(HDR)
	$(CC) -DDEBUG $(CFLAGS) $(SRC)
	ar rs libpajdegD.a *.o

libpajdeg.a: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC)
	ar rs libpajdeg.a *.o

clean:
	rm -f *~ *.o libpajdeg*.a
