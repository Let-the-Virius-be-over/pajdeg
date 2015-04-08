CFLAGS  = -I include -std=gnu99 -c -Wall
CC      = gcc
SRC	= *.c
# PDBTree.c PDEnv.c PDObject.c PDOperator.c PDParser.c PDPipe.c PDPortableDocumentFormatState.c PDReference.c PDScanner.c pd_stack.c PDState.c PDStaticHash.c PDStreamFilter.c PDStreamFilterFlateDecode.c PDStreamFilterPrediction.c PDTask.c PDTwinStream.c PDXTable.c

# HDR	= PDBTree.h PDDefines.h PDEnv.h PDInternal.h PDObject.h PDOperator.h PDPDFPrivate.h PDParser.h PDPipe.h PDPortableDocumentFormatState.h PDReference.h PDScanner.h pd_stack.h PDState.h PDStaticHash.h PDTask.h PDTwinStream.h Pajdeg.h
HDR = *.h


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
