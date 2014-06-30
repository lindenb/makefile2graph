CC=gcc
CFLAGS=-O3 -Wall # -DNO_XML
.PHONY: all clean test
all: make2graph

make2graph: makegraph.c
	$(CC) -o $@ $(CFLAGS) $<

test: make2graph
	$(MAKE) -Bnd $< | ./$< && \
	$(MAKE) -Bnd $< | ./$< -x

clean: 
	
