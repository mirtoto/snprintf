// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com
#ifndef SNPRINTF_H_
#define SNPRINTF_H_


#include <stdarg.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifdef USE_SNPRINTF_PREFIX
#define SNPRINTF_PREFIX(name) my_##name
#else
#define SNPRINTF_PREFIX(name) name
#endif


/** @see snprintf() */
int SNPRINTF_PREFIX(vsnprintf)(char *string, size_t length, const char *format, va_list args)
#if !defined(__MINGW32__)
    __attribute__((format(printf, 3, 0)))
#endif
;

/**
 * Implementation of snprintf() function which create @p string of maximum
 * @p length - 1 according of instruction provided by @p format. 
 * 
 * # Supportted types
 * 
 *  Type    | Description
 * -------- | ----------------------------------------
 *  d / i   | signed decimal integer
 *  u       | unsigned decimal integer
 *  o       | unsigned octal integer
 *  x       | unsigned hexadecimal integer
 *  f / F   | decimal floating point
 *  e / E   | scientific (exponential) floating point
 *  g / G   | scientific or decimal floating point
 *  c       | character
 *  s       | string
 *  p       | pointer
 *  %       | percent character
 * 
 * # Supported lengths
 * 
 *  Length  | Description
 * -------- | ----------------------------------------
 *  hh      | signed / unsigned char
 *  h       | signed / unsigned short
 *  l       | signed / unsigned long
 *  ll      | signed / unsigned long long
 * 
 * # Supported flags
 * 
 *   Flag   | Description
 * -------- | ----------------------------------------
 *  -       | justify left
 *  +       | justify right or put a plus if number
 *  #       | prefix 0x, 0X for hex and 0 for octal
 *  *       | width and/or precision is specified as an int argument
 *  0       | for number padding with zeros instead of spaces
 *  (space) | leave a blank for number with no sign
 * 
 * @param string Output buffer.
 * @param length Size of output buffer @p string.
 * @param format Format of input parameters.
 * @param ... Input parameters according of @p format.
 * 
 * @retval >=0 Amount of characters put in @p string.
 * @retval  -1 Output buffer size is too small.
 */
int SNPRINTF_PREFIX(snprintf)(char *string, size_t length, const char *format, ...)
#if !defined(__MINGW32__)
    __attribute__((format(printf, 3, 4)))
#endif
;


#ifdef USE_SNPRINTF_PREFIX

#ifdef snprintf
#undef snprintf
#endif
#define snprintf SNPRINTF_PREFIX(snprintf)

#ifdef vsnprintf
#undef vsnprintf
#endif
#define vsnprintf SNPRINTF_PREFIX(vsnprintf)

#endif // #ifdef USE_SNPRINTF_PREFIX


#ifdef __cplusplus
}
#endif


#endif  // SNPRINTF_H_
