CFLAGS = -Wall -g
LDFLAGS = -Wl,-soname,libmalloc.so
LDLIBS = -lpthread

libmalloc.so: malloc.c
	$(CC) -o $@ -fPIC -shared $(CFLAGS) $(LDFLAGS) $(LDLIBS) $^
