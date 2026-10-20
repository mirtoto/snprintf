CC		:= gcc
CFLAGS	:= -DUSE_SNPRINTF_PREFIX -Wall -Wextra -g

# Tests intentionally use format strings that exceed INT_MAX to verify
# overflow handling. GCC's -Wformat-overflow cannot be suppressed via
# #pragma because it is evaluated during optimization, so disable it
# for the test file only.
src/tests-snprintf.o: override CFLAGS += -Wno-format-overflow
COVFLAGS	:=

BIN		:= bin
SRC		:= src
INCLUDE	:= include
LIB		:= lib

LIBRARIES	:=

ifeq ($(OS),Windows_NT)
EXECUTABLE	:= main.exe
SOURCEDIRS	:= $(SRC)
INCLUDEDIRS	:= $(INCLUDE)
ifneq ($(wildcard $(LIB)),)
  LIBDIRS		:= $(LIB)
else
  LIBDIRS		:=
endif
MKDIR		:= mkdir
else
EXECUTABLE	:= main
SOURCEDIRS	:= $(shell find $(SRC) -type d)
INCLUDEDIRS	:= $(shell find $(INCLUDE) -type d)
ifneq ($(wildcard $(LIB)),)
  LIBDIRS		:= $(shell find $(LIB) -type d)
else
  LIBDIRS		:=
endif
MKDIR		:= mkdir -p
endif
# Link libm only when the optional math backend is enabled.
ifneq ($(filter -DSNPRINTF_USE_MATH,$(CFLAGS)),)
  LIBRARIES += -lm
endif

CINCLUDES	:= $(patsubst %,-I%, $(INCLUDEDIRS:%/=%))
ifneq ($(strip $(LIBDIRS)),)
CLIBS		:= $(patsubst %,-L%, $(LIBDIRS:%/=%))
else
CLIBS		:=
endif

SOURCES		:= $(wildcard $(patsubst %,%/*.c, $(SOURCEDIRS)))
OBJECTS		:= $(SOURCES:.c=.o)

all: $(BIN)/$(EXECUTABLE)

.PHONY: clean coverage check-headers check-c89
clean:
	-$(RM) $(BIN)/$(EXECUTABLE)
	-$(RM) $(OBJECTS)
	-$(RM) *.gcda *.gcno *.gcov src/*.gcda src/*.gcno

coverage:
	$(MAKE) clean
	$(MAKE) CFLAGS="-DUSE_SNPRINTF_PREFIX -Wall -Wextra -g --coverage" COVFLAGS="--coverage"
	./$(BIN)/$(EXECUTABLE)
	gcov -b -m src/snprintf.c

run: all
	./$(BIN)/$(EXECUTABLE)

# snprintf.c must compile with the standard headers already in scope, as with a
# -include or in a one file build, and, with SNPRINTF_NO_LONGLONG, as C89.
check-headers:
	$(CC) -DUSE_SNPRINTF_PREFIX $(CINCLUDES) -include stdint.h -include stdlib.h -include inttypes.h -c src/snprintf.c -o /dev/null
	$(CC) $(CINCLUDES) -include stdint.h -include sys/types.h -c src/snprintf.c -o /dev/null

check-c89:
	$(CC) -std=c89 -pedantic-errors -Wall -Wextra -DSNPRINTF_NO_LONGLONG -DUSE_SNPRINTF_PREFIX $(CINCLUDES) -c src/snprintf.c -o /dev/null

.c.o:
	$(CC) $(CFLAGS) $(COVFLAGS) $(CINCLUDES) -c $< -o $@

$(BIN)/$(EXECUTABLE): $(OBJECTS) | $(BIN)/
	$(CC) $(CLIBS) $(COVFLAGS) -o $@ $^ $(LIBRARIES)

$(BIN)/:
	$(MKDIR) $@