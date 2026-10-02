# Differential fuzz test of snprintf() against the C library.
#
#   make -C fuzz check                                    default build
#   make -C fuzz check DEFS=-DSNPRINTF_STRICT             strict mode
#   make -C fuzz check DEFS=-DSNPRINTF_USE_MATH           math.h helpers
#   make -C fuzz check SAN= CFLAGS="-O2 -m32"             no sanitizers, 32 bits
#   make -C fuzz check ITERATIONS=1000000 SEEDS="1 2 3"   run longer
#   make -C fuzz run SEED=1234                            reproduce a failure
#
# The binary is built with -DUSE_SNPRINTF_PREFIX, so the C library is the
# reference. The recipes are indented with tabs.

CC ?= cc
CFLAGS ?= -O1 -g -Wall -Wextra
SAN ?= -fsanitize=address,undefined -fno-sanitize-recover=undefined
DEFS ?=
ITERATIONS ?= 200000
SEEDS ?= 1 2 3
SEED ?= 1

fuzz-snprintf: fuzz-snprintf.c ../src/snprintf.c ../include/snprintf.h
	$(CC) $(CFLAGS) $(SAN) -DUSE_SNPRINTF_PREFIX $(DEFS) -I../include -o $@ fuzz-snprintf.c ../src/snprintf.c -lm

check: fuzz-snprintf
	@for seed in $(SEEDS); do ./fuzz-snprintf $$seed $(ITERATIONS) || exit 1; done

run: fuzz-snprintf
	./fuzz-snprintf $(SEED) $(ITERATIONS)

clean:
	rm -f fuzz-snprintf

.PHONY: check run clean
