# Build the xBase interpreter.
#
#   make          builds build/xbase.rxbin from xbase.crexx
#   make gen      regenerates xbase.crexx from src/ (needs python3)
#   make test     runs every program in tests/ and compares the output
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

clean:
	rm -rf build

.PHONY: all gen test clean
