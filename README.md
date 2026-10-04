# snprintf()

Lightweight, dependency-light implementation of the C `snprintf()` family. It is intended for small embedded or minimal builds where a full libc formatter is not available and where avoiding `math.h` is desirable.

The project aims to stay portable and close to libc behavior for the supported conversions, while still keeping the implementation small and easy to embed.

## Features

- portable formatter for common `printf`-style conversions
- formatter implementation has no `math.h` dependency by default
- supports the standard integer, floating-point, string, character, and pointer cases used by this project
- exact floating-point conversions: the digits are those of the binary value, rounded to nearest, ties to even, so the output is the one of libc (see [Floating-point conversions](#floating-point-conversions))
- default behavior is intentionally libc-like and permissive
- optional strict validation mode for safety-oriented builds
- tested against the C library with a differential fuzz test (see [Testing](#testing))

## Function prototype

```c
int snprintf(char *string, size_t length, const char *format, ...);
```

### Parameters

| Parameter | Description                                                                                                                                  |
| --------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| `string`  | Output buffer. If `NULL`, nothing is written, `length` is ignored and the function calculates the output length.                             |
| `length`  | Size of the output buffer, including the terminating null byte: at most `length - 1` characters are written. `0` only measures the output.   |
| `format`  | Format string controlling the output.                                                                               |
| `...`     | Variadic arguments consumed by the format string.                                                                   |

### Return value

| Value  | Meaning                                                                                                                                                  |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `>= 0` | Number of characters of the whole output, not counting the terminating null byte, also when the output is truncated. An output longer than `INT_MAX` is reported as `INT_MAX`. With `SNPRINTF_LEGACY_LENGTH` it is the number of characters written, that is `length - 1`, for a truncated output. |
| `-1`   | A strict-mode validation failure.                                                                                                                        |

This is the same as the C99 `snprintf()` returns, so a caller can tell that the output was truncated and retry with a bigger buffer. To find out the size of a buffer up front, call the function with a `NULL` `string`.

### Optional legacy return value

Before version 3.2 the result of a truncated output was the number of characters written, not the length of the whole output, and `%n` stored that number. Define `SNPRINTF_LEGACY_LENGTH` to keep it, for a project that depends on it:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_LEGACY_LENGTH -Wall -Wextra -g"
```

In that mode nothing more can be written once the buffer is full, so the rest of the format is not processed at all and a `%n` behind that point is never stored. Apart from the returned length and the `%n`, the two modes write the same text into the buffer and leave the same bytes past its end.

The strict mode is the exception, because it always processes the whole format to validate it: also with `SNPRINTF_LEGACY_LENGTH` a `%n` behind a full buffer is reached, and stores the number of characters written.

## Usage

Copy `src/snprintf.c` and `include/snprintf.h` into your project and include `snprintf.h`.

```c
#include <stdio.h>
#include "snprintf.h"

int main(void) {
  const char *hello = "Hello";
  const char *world = "World";
  char msg[128] = {0};

  snprintf(msg, sizeof(msg), "%s %s!", hello, world);
  printf("%s\n", msg);
  return 0;
}
```

## Compatibility model

The default implementation is intentionally permissive and mirrors libc fallback behavior for malformed format strings when possible. This makes it behave like a lightweight libc-compatible formatter instead of a strict custom validator.

In particular:

- the parser is permissive rather than fully strict `printf`-validator behavior
- malformed or unsupported specifiers often fall back in a libc-like way
- the design favors compatibility over custom safety checks
- mismatched argument types remain undefined behavior, matching standard `printf` semantics

This is a good default for portability and compatibility, but it is not a safety-oriented validation layer.

### Differences to the C library

- `%g` and `%G` with the `#` flag always print as many significant digits as the precision says, also when rounding makes the number a power of ten, as the C standard requires: `%#g` of `999999.5` is `1.00000e+06`, where glibc prints `1.e+06`.
- `%s` of a `NULL` pointer prints `(null)`, whole or cut by the precision, but a precision below its 6 characters prints nothing at all, which is what glibc does.
- The sign of a NaN is not printed, so a negative one gives `nan`, where glibc gives `-nan`. An infinity keeps its sign.
- The length modifier `l` is ignored by `%s` and `%c`: there are no wide characters. The strict mode rejects a length modifier on both of them, and on `%s` also the flags the standard does not define there, and returns `-1`.
- The precision is limited, see [Width and precision](#width-and-precision).
- The integral part of a floating-point number is limited to `SNPRINTF_FLOAT_INTEGRAL_DIGITS` digits, 309 by default and enough for every `double`. A smaller value saves stack, and a number with a longer integral part is then printed as a row of nines, with the exponent of `%e` and `%g` taken from the biggest number that fits, see [Floating-point conversions](#floating-point-conversions).
- Not supported are the length modifier `L` (`long double`), the conversions `a` and `A`, the extensions of other C libraries like `%'` for grouping and `%m` for the `strerror` string, and numbered arguments like `%1$d`. Only the `%` is printed: everything from it to the unsupported character goes with it, and the rest of the format is text, so `%5k` prints `%`, `%La` prints `%a` and `%1$d` prints `%d`. No argument is used, so a conversion after it reads the argument the unsupported one would have had. In strict mode the function returns `-1`.

### Optional strict mode

For stricter validation, compile with `-DSNPRINTF_STRICT`.

When enabled, malformed or unsupported format specifiers return `-1` instead of falling back in permissive mode.

For example, enable it for the project build with:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_STRICT -Wall -Wextra -g"
```

This preserves the default compatibility model while giving embedded or security-sensitive builds an explicit safety option.

### Optional math library support

The floating-point conversion is self-contained and does not require `math.h` or libm. Define `SNPRINTF_USE_MATH` to use `modf()` and `signbit()` from `math.h` instead of the built-in helpers. On toolchains where these functions are provided by a separate math library, link with `-lm`, which the Makefile of this project adds for that backend on its own:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_USE_MATH -Wall -Wextra -g"
```

### Configuration macros

| Macro                            | Description                                                                                                                                                                              |
| -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `USE_SNPRINTF_PREFIX`            | Name the functions `my_snprintf()` and `my_vsnprintf()` and define `snprintf` and `vsnprintf` as macros for them, to link with the C library. The tests need it.                         |
| `SNPRINTF_STRICT`                | Return `-1` for a malformed or unsupported specifier.                                                                                                                                    |
| `SNPRINTF_LEGACY_LENGTH`         | Return and store the number of characters written into the buffer instead of the length of the whole output, as before version 3.2.                                                |
| `SNPRINTF_USE_MATH`              | Use `modf()` and `signbit()` of `math.h`.                                                                                                                                                |
| `SNPRINTF_FLOAT_INTEGRAL_DIGITS` | Digits of the integral part of a `double`, `309` by default, which is enough for every `double`. A smaller number saves stack, but bigger numbers are then printed as a row of nines. Set it when compiling `snprintf.c`. |
| `SNPRINTF_FLOAT_PRECISION`       | Digits of the precision of `%e`, `%E`, `%f`, `%F`, `%g` and `%G`, `29` by default. The C standard asks for at least `999`, so set it higher, e.g. to `999`, to be conformant. The stack grows by about 3 bytes per digit for `%e` and `%g`, and by about 1 for `%f`. A bigger precision is lowered to this one, or fails with `-1` in the strict mode. Set it when compiling `snprintf.c`. |

## Supported format specifiers

### Types

| Type      | Description                                                    |
| --------- | -------------------------------------------------------------- |
| `d` / `i` | signed decimal integer                                         |
| `u`       | unsigned decimal integer                                       |
| `o`       | unsigned octal integer                                         |
| `x` / `X` | unsigned hexadecimal integer                                   |
| `f` / `F` | decimal floating point                                         |
| `e` / `E` | scientific notation                                            |
| `g` / `G` | shortest of `%e` and `%f`                                      |
| `c`       | character                                                      |
| `s`       | string, `(null)` for a `NULL` pointer                          |
| `p`       | pointer, `(nil)` for a `NULL` pointer                          |
| `n`       | length of the whole output so far, see [Optional legacy return value](#optional-legacy-return-value) |
| `%`       | percent sign                                                   |

### Length modifiers

| Modifier | Description                                  |
| -------- | -------------------------------------------- |
| `hh`     | `signed char` / `unsigned char`              |
| `h`      | `short` / `unsigned short`                   |
| `l`      | `long` / `unsigned long`                     |
| `ll`     | `long long` / `unsigned long long`           |
| `z`      | `size_t` / signed `size_t`                   |
| `t`      | `ptrdiff_t` / unsigned `ptrdiff_t`           |
| `j`      | `intmax_t` / `uintmax_t`                     |

### Flags

| Flag  | Description                                                                                          |
| ----- | ---------------------------------------------------------------------------------------------------- |
| `-`   | left-justify                                                                                         |
| `+`   | force a leading plus sign for positive signed numbers                                                |
| `#`   | alternate form (`0x`, `0X`, `0`), keep the point and the trailing zeros of floating-point numbers    |
| `*`   | width and/or precision supplied as an `int` argument                                                 |
| `0`   | zero-pad numeric output, ignored with the `-` flag and with the precision of an integer              |
| space | prefix a blank for positive signed values, ignored with the `+` flag                                 |

### Width and precision

Both are decimal numbers or a star. A negative width from a star is the `-` flag with the width made positive, a negative precision from a star is no precision, and a lone dot is the precision `0`.

| Conversion           | Precision                                                                  |
| -------------------- | -------------------------------------------------------------------------- |
| integers             | minimal number of digits, the digits and the sign are not cut             |
| `f` / `F`            | digits after the point, 6 by default, `SNPRINTF_FLOAT_PRECISION` at most  |
| `e` / `E`            | digits after the point, 6 by default, `SNPRINTF_FLOAT_PRECISION` at most  |
| `g` / `G`            | significant digits, 6 by default, `SNPRINTF_FLOAT_PRECISION` at most       |
| `s`                  | maximal number of characters                                               |

A bigger precision of a finite floating-point conversion is lowered to `SNPRINTF_FLOAT_PRECISION`, which is 29 by default, or fails with `-1` in the strict mode. An infinity and a not-a-number have no digits, so their precision changes nothing and is never lowered or refused. The C standard asks for at least 999 digits, so compile with `-DSNPRINTF_FLOAT_PRECISION=999` to be conformant.

### Floating-point conversions

The digits are those of the exact binary value of the `double`, rounded to nearest, ties to even, like the C library does: `%.2f` of `1.005` is `1.00` and `%.0f` of `2.5` is `2`. Every finite `double` is supported, subnormals too, as long as the integral part has no more than `SNPRINTF_FLOAT_INTEGRAL_DIGITS` digits, which is enough for all of them by default.

Infinity is printed as `inf` and not a number as `nan`, in capitals for `F`, `E` and `G`, and the `0` flag is ignored for them. The sign of NaN is not printed. Negative zero is printed with a minus sign.

The conversion uses integer arithmetic only. Its cost is some stack: `floating()` needs about 600 bytes with the default 309 digits, and about 300 bytes with `-DSNPRINTF_FLOAT_INTEGRAL_DIGITS=40`. `SNPRINTF_FLOAT_PRECISION` adds about 3 bytes per digit on top of that for `%e` and `%g`, and about 1 for `%f`, so `-DSNPRINTF_FLOAT_PRECISION=999` needs about 3.7 kB for `%e` and for `%g` alike: `%g` measures the exponent first, but that call has returned before the digits are printed. These numbers were measured with gcc, another compiler may need a little more or less.

## Testing

The project includes a small MinUnit-based suite in `src/tests-snprintf.c` and `include/tests-snprintf.h`. It needs `-DUSE_SNPRINTF_PREFIX`, because it calls the functions of this project next to the ones of the C library.

```c
#include "tests-snprintf.h"

int main(void) {
  return tests_snprintf();
}
```

The `fuzz` directory has a differential fuzz test: every random format, with random flags, width, precision, length modifiers and values, is run through the C library and through this implementation, and the output, the returned length, the length for a `NULL` buffer and the truncated output are compared. The formats it leaves out are the differences listed above, plus `%n` and a few cases the C standard does not define, all listed at the top of `fuzz-snprintf.c`.

```sh
make -C fuzz check                                   # 3 seeds, 200000 formats each, with sanitizers
make -C fuzz check DEFS=-DSNPRINTF_STRICT            # also with -DSNPRINTF_LEGACY_LENGTH
make -C fuzz check DEFS=-DSNPRINTF_USE_MATH           # a smaller -DSNPRINTF_FLOAT_INTEGRAL_DIGITS reports differences
make -C fuzz check SAN= CFLAGS="-O2 -m32"            # 32 bits, without sanitizers
make -C fuzz run SEED=1234 ITERATIONS=1000000        # reproduce a failure
```

The unit tests and the fuzz test run in CI for gcc and clang, in all four builds (the default one, `SNPRINTF_STRICT`, `SNPRINTF_LEGACY_LENGTH` and `SNPRINTF_USE_MATH`), and in 32 bits for `-m32` and `-m32 -msse2 -mfpmath=sse`, see `.github/workflows/ci.yml`.

## Authors

- Mirosław Toton, <mirtoto@gmail.com>
- Alain Magloire, <alainm@rcsm.ee.mcgill.ca>
