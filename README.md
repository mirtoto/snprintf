# snprintf()

Lightweight, dependency-light implementation of the C `snprintf()` family. It is intended for small embedded or minimal builds where a full libc formatter is not available and where avoiding `math.h` is desirable.

The project aims to stay portable and close to libc behavior for the supported conversions, while still keeping the implementation small and easy to embed.

## Features

- portable formatter for common `printf`-style conversions
- formatter implementation has no `math.h` dependency by default
- supports the standard integer, floating-point, string, character, and pointer cases used by this project
- default behavior is intentionally libc-like and permissive
- optional strict validation mode for safety-oriented builds

## Function prototype

```c
int snprintf(char *string, size_t length, const char *format, ...);
```

### Parameters

| Parameter | Description |
| --------- | ----------- |
| `string` | Output buffer. If `NULL`, the function calculates the required output length. |
| `length` | Capacity of the output buffer, excluding the terminating null byte. |
| `format` | Format string controlling the output. |
| `...` | Variadic arguments consumed by the format string. |

### Return value

| Value | Meaning |
| ----- | ------- |
| `>= 0` | Number of characters written, or the number that would have been written when `string` is `NULL`. |
| `-1` | Invalid buffer size or a strict-mode validation failure. |

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

### Optional strict mode

For stricter validation, compile with `-DSNPRINTF_STRICT`.

When enabled, malformed or unsupported format specifiers return `-1` instead of falling back in permissive mode.

For example, enable it for the project build with:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_STRICT -Wall -Wextra -g"
```

This preserves the default compatibility model while giving embedded or security-sensitive builds an explicit safety option.

### Optional math library support

The default floating-point conversion uses self-contained helpers and does not require `math.h` or libm. Define `SNPRINTF_USE_MATH` to use `pow()`, `log10()`, `floor()`, `fabs()`, and `modf()` from `math.h` instead. On toolchains where these functions are provided by a separate math library, link with `-lm`:

```sh
make CFLAGS="-DUSE_SNPRINTF_PREFIX -DSNPRINTF_USE_MATH -Wall -Wextra -g" LIBRARIES="-lm"
```

## Supported format specifiers

### Types

| Type | Description |
| ---- | ----------- |
| `d` / `i` | signed decimal integer |
| `u` | unsigned decimal integer |
| `o` | unsigned octal integer |
| `x` / `X` | unsigned hexadecimal integer |
| `f` / `F` | decimal floating point |
| `e` / `E` | scientific notation |
| `g` / `G` | shortest of `%e` and `%f` |
| `c` | character |
| `s` | string |
| `p` | pointer |
| `%` | percent sign |

### Length modifiers

| Modifier | Description |
| -------- | ----------- |
| `hh` | `signed char` / `unsigned char` |
| `h` | `short` / `unsigned short` |
| `l` | `long` / `unsigned long` |
| `ll` | `long long` / `unsigned long long` |

### Flags

| Flag | Description |
| ---- | ----------- |
| `-` | left-justify |
| `+` | force a leading plus sign for positive numbers |
| `#` | alternate form (`0x`, `0X`, `0`) |
| `*` | width and/or precision supplied as an `int` argument |
| `0` | zero-pad numeric output |
| space | prefix a blank for positive signed values |

## Testing

The project includes a small MinUnit-based suite in `src/tests-snprintf.c` and `include/tests-snprintf.h`.

```c
#include "tests-snprintf.h"

int main(void) {
  return tests_snprintf();
}
```

## Authors

- Mirosław Toton, mirtoto@gmail.com
- Alain Magloire, alainm@rcsm.ee.mcgill.ca
