// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com
#ifndef SNPRINTF_H_
#define SNPRINTF_H_


#include <stdarg.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


/** @see snprintf() */
int vsnprintf(char *string, size_t length, const char *format, va_list args) __attribute__((format(printf, 3, 0)));

/**
 * Implementation of snprintf() function which create @p string of maximum
 * @p length - 1 according of instruction provided by @p format. 
 * 
 * Function support:
 *  - Integer numbers:
 *      - Unsigned: %hu %llu %lu %u
 *      - Decimal: %hd %lld %ld %d
 *      - Octal: %ho %llo %lo %o
 *      - Hexa: %hx %llx %lx %x %X
 *  - Floating point numbers:
 *      - Double: %g %G %e %E %f
 *  - Strings:
 *      - String: %s
 *      - Character: %c
 *      - Percent character: %%
 *  - Other:
 *      - Pointer: %p
 *
 * Formating conversion flags:
 *  -  '-' justify left
 *  -  '+' justify right or put a plus if number
 *  -  '#' prefix 0x, 0X for hex and 0 for octal
 *  -  '*' precision/witdth is specify as an (int) in the arguments
 *  -  ' ' leave a blank for number with no sign
 *  -  'l' integer parameter should be a long
 *  - 'll' integer parameter should be a long long
 *  -  'h' integer parameter should be a short
 *  - 'hh' integer parameter should be a char
 * 
 * @param string Output buffer.
 * @param length Size of output buffer @p string.
 * @param format Format of input parameters.
 * @param ... Input parameters according of @p format.
 * 
 * @retval >=0 Amount of characters put in @p string.
 * @retval  -1 Output buffer size is too small.
 */
int snprintf(char *string, size_t length, const char *format, ...) __attribute__((format(printf, 3, 4)));


#ifdef __cplusplus
}
#endif


#endif  // SNPRINTF_H_
