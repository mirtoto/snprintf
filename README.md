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

| Parameter | Description                                                                                                         |
| --------- | ------------------------------------------------------------------------------------------------------------------- |
| `string`  | Output buffer. If `NULL`, nothing is written, `length` is ignored and the function calculates the output length.    |
| `length`  | Size of the output buffer, including the terminating null byte: at most `length - 1` characters are written.        |
| `format`  | Format string controlling the output.                                                                               |
| `...`     | Variadic arguments consumed by the format string.                                                                   |

### Return value

| Value  | Meaning                                                                                                                                                  |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `>= 0` | Number of characters of the whole output, not counting the terminating null byte, also when the output is truncated. An output longer than `INT_MAX` is reported as `INT_MAX`. With `SNPRINTF_LEGACY_LENGTH` it is the number of characters written, that is `length - 1`, for a truncated output. |
| `-1`   | `string` is not `NULL` and `length` is `0`, or a strict-mode validation failure.                                                                         |

This is the same as the C99 `snprintf()` returns, so a caller can tell that the output was truncated and retry with a bigger buffer. To find out the size of a buffer up front, call the function with a `NULL` `string`.

### Optional legacy return value

Before version 3.2 the result of a truncated output was the number of characters written, not the length of the whole output, and `%n` stored that number. Define `SNPRINTF_LEGACY_LENGTH` to keep it, for a project that depends on it:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_LEGACY_LENGTH -Wall -Wextra -g"
```

The two modes differ only in the returned length and in what `%n` stores. The text written into the buffer, and the bytes past its end, are the same in both.

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

- `%g` and `%G` with the `#` flag always print as many significant digits as the precision says, also when rounding makes the number a power of ten, as the C standard requires (glibc prints `1.e+06` for `999999.5`).
- `%s` of a `NULL` pointer prints `(null)`, cut by the precision.
- The length modifier `l` is ignored by `%s` and `%c`: there are no wide characters.
- The precision is limited, see [Width and precision](#width-and-precision).
- Not supported are the length modifier `L` (`long double`), the conversions `a` and `A`, and numbered arguments like `%1$d`. A percent character is printed, the unsupported character is skipped, the rest is printed as text and no argument is used. In strict mode the function returns `-1`.

### Optional strict mode

For stricter validation, compile with `-DSNPRINTF_STRICT`.

When enabled, malformed or unsupported format specifiers return `-1` instead of falling back in permissive mode.

For example, enable it for the project build with:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_STRICT -Wall -Wextra -g"
```

This preserves the default compatibility model while giving embedded or security-sensitive builds an explicit safety option.

### Optional math library support

The floating-point conversion is self-contained and does not require `math.h` or libm. Define `SNPRINTF_USE_MATH` to use `modf()` and `signbit()` from `math.h` instead of the built-in helpers. On toolchains where these functions are provided by a separate math library, link with `-lm`:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_USE_MATH -Wall -Wextra -g" LIBRARIES="-lm"
```

### Configuration macros

| Macro                            | Description                                                                                                                                                                              |
| -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `USE_SNPRINTF_PREFIX`            | Name the functions `my_snprintf()` and `my_vsnprintf()` and define `snprintf` and `vsnprintf` as macros for them, to link with the C library. The tests need it.                         |
| `SNPRINTF_STRICT`                | Return `-1` for a malformed or unsupported specifier.                                                                                                                                    |
| `SNPRINTF_LEGACY_LENGTH`         | Return and store the number of characters written into the buffer instead of the length of the whole output, as before version 3.2.                                                |
| `SNPRINTF_USE_MATH`              | Use `modf()` and `signbit()` of `math.h`.                                                                                                                                                |
| `SNPRINTF_FLOAT_INTEGRAL_DIGITS` | Digits of the integral part of a `double`, `309` by default, which is enough for every `double`. A smaller number saves stack, but bigger numbers are then printed as a row of nines. Set it when compiling `snprintf.c`. |

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
| `n`       | store the number of characters written so far                  |
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
| integers             | minimal number of digits, the digits and the sign are cut to 99 characters |
| `f` / `F`            | digits after the point, 6 by default, 29 at most                           |
| `e` / `E`            | digits after the point, 6 by default, 29 at most                           |
| `g` / `G`            | significant digits, 6 by default, 29 at most                               |
| `s`                  | maximal number of characters                                               |

A bigger precision of a floating-point conversion is lowered to 29.

### Floating-point conversions

The digits are those of the exact binary value of the `double`, rounded to nearest, ties to even, like the C library does: `%.2f` of `1.005` is `1.00` and `%.0f` of `2.5` is `2`. Every finite `double` is supported, subnormals too, as long as the integral part has no more than `SNPRINTF_FLOAT_INTEGRAL_DIGITS` digits, which is enough for all of them by default.

Infinity is printed as `inf` and not a number as `nan`, in capitals for `F`, `E` and `G`, and the `0` flag is ignored for them. The sign of NaN is not printed. Negative zero is printed with a minus sign.

The conversion uses integer arithmetic only. Its cost is some stack: `floating()` needs about 600 bytes with the default 309 digits, and about 250 bytes with `-DSNPRINTF_FLOAT_INTEGRAL_DIGITS=40`.

## Testing

The project includes a small MinUnit-based suite in `src/tests-snprintf.c` and `include/tests-snprintf.h`. It needs `-DUSE_SNPRINTF_PREFIX`, because it calls the functions of this project next to the ones of the C library.

```c
#include "tests-snprintf.h"

int main(void) {
  return tests_snprintf();
}
```

The `fuzz` directory has a differential fuzz test: every random format, with random flags, width, precision, length modifiers and values, is run through the C library and through this implementation, and the output, the returned length, the length for a `NULL` buffer and the truncated output are compared. The formats it leaves out are the differences listed above.

```sh
make -C fuzz check                                   # 3 seeds, 200000 formats each, with sanitizers
make -C fuzz check DEFS=-DSNPRINTF_STRICT            # also with -DSNPRINTF_LEGACY_LENGTH
make -C fuzz check DEFS=-DSNPRINTF_USE_MATH           # and with -DSNPRINTF_FLOAT_INTEGRAL_DIGITS=40
make -C fuzz check SAN= CFLAGS="-O2 -m32"            # 32 bits, without sanitizers
make -C fuzz run SEED=1234 ITERATIONS=1000000        # reproduce a failure
```

The unit tests and the fuzz test run in CI for gcc and clang, in all three builds, and in 32 bits, see `.github/workflows/ci.yml`.

## Authors

- Mirosław Toton, <mirtoto@gmail.com>
- Alain Magloire, <alainm@rcsm.ee.mcgill.ca>
