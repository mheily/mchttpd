BINDIR =	/usr/local/sbin
MANDIR =	/usr/local/man/man8
CC =		cc
CFLAGS =	-O -std=c99 -Wall -Wpointer-arith -Wshadow -Wcast-qual -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wno-long-long
CFLAGS += 	-DUSE_CAPSICUM=1 -Werror
#SYSVLIBS =	-lnsl -lsocket
LDFLAGS =	-s $(SYSVLIBS)

all:		micro_httpd micro_httpd_debug

micro_httpd_debug:	micro_httpd.c cap_stub.c
	$(CC) $(CFLAGS) -O0 -g -o micro_httpd_debug *.c

micro_httpd:	micro_httpd.o cap_stub.o
	$(CC) micro_httpd.o cap_stub.o $(LDFLAGS) -o micro_httpd

micro_httpd.o:	micro_httpd.c
	$(CC) $(CFLAGS) -c micro_httpd.c

cap_stub.o:	cap_stub.c
	$(CC) $(CFLAGS) -c cap_stub.c
	
install:	all
	rm -f $(BINDIR)/micro_httpd
	cp micro_httpd $(BINDIR)/micro_httpd
	rm -f $(MANDIR)/micro_httpd.8
	cp micro_httpd.8 $(MANDIR)/micro_httpd.8

clean:
	rm -f micro_httpd micro_httpd_debug *.o core core.* *.core hello_world

check:	micro_httpd_debug
	# Test 1 - a simple file
	echo 'hello world' > hello_world
	printf 'GET /hello_world HTTP/1.0\r\n\r\n' | ./micro_httpd_debug `pwd`
	rm hello_world
	
	# Test 2 - a directory
	printf 'GET / HTTP/1.0\r\n\r\n' | ./micro_httpd_debug `pwd`
	
