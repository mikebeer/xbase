# Build the xBase interpreter.
#
#   make          builds build/xbase.rxbin from xbase.crexx
#   make gen      regenerates xbase.crexx from src/ (needs python3)
#   make test     runs every program in tests/ and compares the output
#   make c        builds the C89 version, c/xbase-c
#   make test-c   runs the test programs with the C89 version
#
# CREXX_BIN is the directory holding the cREXX tools (rxc, rxas, rxvm).
CREXX_BIN ?= $(dir $(shell which rxc))
RXC  = $(CREXX_BIN)/rxc
RXAS = $(CREXX_BIN)/rxas

all: build/xbase.rxbin

build/xbase.rxbin: xbase.crexx
	mkdir -p build
	cp xbase.crexx build/xbase.crexx
	cd build && $(RXC) -n xbase > xbase.log 2>&1 || (grep -i error xbase.log; exit 1)
	cd build && $(RXAS) xbase

gen:
	python3 src/gen.py xbase.crexx

test: all
	./tests/run.sh

c:
	$(MAKE) -C c

test-c: c
	XB=$(CURDIR)/c/xbase-c ./tests/run.sh

clean:
	rm -rf build
	$(MAKE) -C c clean

.PHONY: all gen test c test-c clean
