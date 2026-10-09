CC		:= gcc
CFLAGS	:= -DUSE_SNPRINTF_PREFIX -Wall -Wextra -g

# Tests intentionally use format strings that exceed INT_MAX to verify
# overflow handling. GCC's -Wformat-overflow cannot be suppressed via
# #pragma because it is evaluated during optimization, so disable it
# for the test file only.
src/tests-snprintf.o: CFLAGS += -Wno-format-overflow
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
LIBDIRS		:= $(LIB)
MKDIR		:= mkdir
else
EXECUTABLE	:= main
SOURCEDIRS	:= $(shell find $(SRC) -type d)
INCLUDEDIRS	:= $(shell find $(INCLUDE) -type d)
LIBDIRS		:= $(shell find $(LIB) -type d)
MKDIR		:= mkdir -p
endif
# Link libm only when the optional math backend is enabled.
ifneq ($(filter -DSNPRINTF_USE_MATH,$(CFLAGS)),)
  LIBRARIES += -lm
endif

CINCLUDES	:= $(patsubst %,-I%, $(INCLUDEDIRS:%/=%))
CLIBS		:= $(patsubst %,-L%, $(LIBDIRS:%/=%))

SOURCES		:= $(wildcard $(patsubst %,%/*.c, $(SOURCEDIRS)))
OBJECTS		:= $(SOURCES:.c=.o)

all: $(BIN)/$(EXECUTABLE)

.PHONY: clean coverage
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

.c.o:
	$(CC) $(CFLAGS) $(COVFLAGS) $(CINCLUDES) -c $< -o $@

$(BIN)/$(EXECUTABLE): $(OBJECTS) | $(BIN)/
	$(CC) $(CLIBS) $(COVFLAGS) -o $@ $^ $(LIBRARIES)

$(BIN)/:
	$(MKDIR) $@