// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com
#ifndef SNPRINTF_H_
#define SNPRINTF_H_


#include <stdarg.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Optional strict validation mode.
 *
 * When defined at compile time, malformed or unsupported format specifiers
 * return -1 instead of falling back in libc-like permissive mode.
 * The default behavior remains backward-compatible and libc-like.
 */
#ifdef SNPRINTF_STRICT
#define SNPRINTF_STRICT_MODE 1
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
 * Implementation of snprintf() function which writes up to @p length - 1
 * characters to @p string according to the instructions in @p format.
 *
 * If @p string is NULL, the function calculates the required output length.
 *
 * # Supported types
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
 * @retval >=0 Number of characters that would be written, or that were written
 *             when @p string is not NULL.
 * @retval  -1 Output buffer is too small or invalid for the requested length.
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
