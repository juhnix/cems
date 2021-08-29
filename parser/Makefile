
CFLAGS = -Wall -g -Os
CC     = gcc $(CFLAGS)
#LDFLAGS = -g --retain-symbols-file=globals
LDFLAGS = -g
PREFIX = /usr

.PHONY: all
.PHONY: examples
.PHONY: lib
.PHONY: docs

#all: lib examples docs
all: lib

docs:
	make -C docs

lib: parser.a parser.so parser.o

.PHONY: install

install: all
	install parser.a $(PREFIX)/lib/libparser.a
	install parser.o $(PREFIX)/lib/parser.o
	install parser.so $(PREFIX)/lib/libparser.so.0.6
	install -m 0744 parser.h $(PREFIX)/include/parser.h
	rm -f $(PREFIX)/lib/libparser.so
	ln -s $(PREFIX)/lib/libparser.so.0.6 $(PREFIX)/lib/libparser.so

parser.so: file.o transform.o parse.o modify.o
	ld $(LDFLAGS) --shared file.o transform.o parse.o modify.o -o parser.so 

parser.o: file.o transform.o parse.o modify.o
	ld $(LDFLAGS) --relocatable file.o transform.o parse.o modify.o -o parser.o
	cp parser.o examples/

parser.a: file.o transform.o parse.o modify.o
	ar -rcs parser.a file.o transform.o parse.o modify.o

file.o: file.c parser.h parser_local.h
	$(CC) -c file.c

transform.o: transform.c parser.h parser_local.h
	$(CC) -c transform.c

modify.o: modify.c parser.h parser_local.h
	$(CC) -c modify.c

parse.o: parse.c parser.h parser_local.h
	$(CC) -c parse.c

examples:
	make -C examples

.PHONY: clean

clean:
	rm -f *.o
	rm -f *.so
	rm -f *.a
	make -C docs clean
	make -C examples clean

