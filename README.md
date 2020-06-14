# snprintf()
Lightweight and with minimal dependencies implementation of C function snprintf() for toolchains without supporting of this function:

```c
int snprintf(char *string, size_t length, const char *format, ...);
```

This project is based on work of Alain Magloire, alainm@rcsm.ee.mcgill.ca.

## How to use it

Just copy `snprintf.c` & `snprintf.h` to your project and include `snprintf.h` in your code.

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

In `tests-snprintf.c` & `tests-snprintf.h` you can find set of tests based on [MinUnit](https://github.com/siu/minunit) engine, which can be run by `tests_snprintf()` function.

```c
#include "tests-snprintf.h"

int main(void) {
  return tests_snprintf();
}
```

## Authors

* Mirosław Toton, mirtoto@gmail.com
* Alain Magloire, alainm@rcsm.ee.mcgill.ca
