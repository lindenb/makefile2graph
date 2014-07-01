CC=gcc
CFLAGS=-O3 -Wall
.PHONY: all clean test
all: make2graph

make2graph: makegraph.c
	$(CC) -o $@ $(CFLAGS) $<

test: make2graph
	$(MAKE) -Bnd $< | ./$< | dot && \
	$(MAKE) -Bnd $< | ./$< -x

clean: 
	rm -f make2graph

