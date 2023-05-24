CC=gcc
OBJECTS = adler32.o crc32.o gzclose.o gzread.o infback.o inflate.o trees.o zutil.o \
	compress.o deflate.o gzlib.o gzwrite.o inffast.o inftrees.o uncompr.o
all: libz.so
.PHONY: all clean
%.o: %.c
	$(CC) $< -o $@ -fPIC -O3 -g -c -fno-stack-protector
libz.so: $(OBJECTS)
	$(CC) $(OBJECTS) -Wl,-z,relro,-z,now -shared -o libz.so
clean:
	rm -rf libz.so $(OBJECTS)
