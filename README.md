# snprintf()
Lightweight, dependency-light implementation of the `snprintf()` C function. It avoids the `math.h` functions that are not always available on embedded platforms.

For many years I relied on the original implementation from Alain Magloire (v1.1), but I eventually needed a replacement that is more closely aligned with standard-library behavior.

## Function prototype

```c
int snprintf(char *string, size_t length, const char *format, ...);
```

### Input parameters

|  Parameter  | Description
| ----------- | ----------------------------------------
|  `string`   | Output buffer.
|  `length`   | Size of output buffer `string`.
|  `format`   | Format of input parameters.
|  `...`      | Input parameters according to `format`.

### Return values

|  Value      | Description
| ----------- | ----------------------------------------
|  >=0        | Amount of characters put (or would be put in case of `string` is set to `NULL`) in `string`.
|  -1         | Output buffer `string` size is too small.

## How to use it

Just copy `src/snprintf.c` & `include/snprintf.h` to your project and include `snprintf.h` in your code.

```c
#include <stdio.h>

#include "snprintf.h"
  
int main(int argc, char *argv[]) {
  const char *hello = "Hello", *world = "World";
  char msg[0x100] = "";

  snprintf(msg, sizeof(msg), "%s %s!", hello, world);
  printf("%s\n", msg);
  
  return 0;
}
 ```

In `src/tests-snprintf.c` & `include/tests-snprintf.h` you can find set of tests based on [MinUnit](https://github.com/siu/minunit) engine, which can be run by `tests_snprintf()` function.

```c
#include "tests-snprintf.h"

int main(void) {
  return tests_snprintf();
}
```

## Standard-compatible behavior

The formatter aims to follow libc behavior as closely as possible for valid format strings and common malformed cases.

In particular:

- supported conversions match the expected libc output for the types this implementation covers
- malformed or unknown conversion specifiers fall back in a libc-like way instead of introducing stricter custom validation
- mismatched argument types remain undefined behavior, just as with the standard printf family

This keeps the implementation portable and behaviorally close to the C library without inventing a custom error policy.

## Supported format specifiers

### Supported types
 
|  Type    | Description
| -------- | ----------------------------------------
|  d / i   | signed decimal integer
|  u       | unsigned decimal integer
|  o       | unsigned octal integer
|  x / X   | unsigned hexadecimal integer
|  f / F   | decimal floating point
|  e / E   | scientific (exponential) floating point
|  g / G   | scientific or decimal floating point
|  c       | character
|  s       | string
|  p       | pointer
|  %       | percent character
 
### Supported lengths
 
|  Length  | Description
| -------- | ----------------------------------------
|  hh      | signed / unsigned char
|  h       | signed / unsigned short
|  l       | signed / unsigned long
|  ll      | signed / unsigned long long
 
### Supported flags
 
|   Flag   | Description
| -------- | ----------------------------------------
|  -       | justify left
|  +       | justify right or put a plus if number
|  #       | prefix 0x, 0X for hex and 0 for octal
|  *       | width and/or precision is specified as an int argument
|  0       | for number padding with zeros instead of spaces
|  (space) | leave a blank for number with no sign

## Authors

* Mirosław Toton, mirtoto@gmail.com
* Alain Magloire, alainm@rcsm.ee.mcgill.ca
