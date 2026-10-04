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
 * characters to @p string according to the instructions in @p format, and
 * always ends the output with a terminating '\0'.
 *
 * If @p string is NULL nothing is written, @p length is ignored and the
 * function calculates the length of the whole output.
 *
 * The output is the one of the C library, with the differences listed below.
 * Floating-point conversions are exact: the digits are those of the binary
 * value of the double, rounded to nearest, ties to even, and no math library
 * is needed.
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
 *  s       | string, (null) for a NULL pointer
 *  p       | pointer, (nil) for a NULL pointer
 *  n       | store the length of the whole output so far, see below
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
 *  z       | size_t / signed size_t
 *  t       | ptrdiff_t / unsigned ptrdiff_t
 *  j       | intmax_t / uintmax_t
 * 
 * # Supported flags
 * 
 *   Flag   | Description
 * -------- | ----------------------------------------
 *  -       | justify left
 *  +       | put a plus before a positive signed number
 *  #       | prefix 0x, 0X for hex and 0 for octal, keep the point and the
 *          | trailing zeros of floating-point numbers
 *  *       | width and/or precision is specified as an int argument
 *  0       | for number padding with zeros instead of spaces, ignored with
 *          | the - flag and with the precision of an integer
 *  (space) | leave a blank for number with no sign, ignored with the + flag
 * 
 * # Width and precision
 * 
 * Both are decimal numbers or a star. A negative width from a star is the
 * - flag with the width made positive, a negative precision from a star is
 * no precision, and a lone dot is the precision 0.
 * 
 *  Conversion  | Precision
 * ------------ | ----------------------------------------
 *  integers    | minimal number of digits, the digits and the sign are not cut
 *  f / F       | digits after the point, 6 by default,
 *              | SNPRINTF_FLOAT_PRECISION at most
 *  e / E       | digits after the point, 6 by default,
 *              | SNPRINTF_FLOAT_PRECISION at most
 *  g / G       | significant digits, 6 by default,
 *              | SNPRINTF_FLOAT_PRECISION at most
 *  s           | maximal number of characters
 * 
 * A bigger precision of a finite floating-point conversion is lowered to
 * SNPRINTF_FLOAT_PRECISION, or fails with -1 in the strict mode. The precision of
 * an infinity or a not-a-number changes nothing, so it is never lowered.
 * 
 * # Floating-point conversions
 * 
 * Every finite double is supported, subnormals too: the integral part may
 * have as many digits as SNPRINTF_FLOAT_INTEGRAL_DIGITS, 309 by default,
 * which is enough for the biggest double. Infinity is printed as inf and
 * not a number as nan, in capitals for F, E and G, and then the 0 flag is
 * ignored. The sign of NaN is not printed, and negative zero keeps a minus
 * sign.
 * 
 * # Differences to the C library
 * 
 *  - %g and %G with the # flag always print as many significant digits as the
 *    precision says, also when rounding makes the number a power of ten, as
 *    the C standard requires: %#g of 999999.5 is 1.00000e+06, where glibc
 *    prints 1.e+06.
 *  - %s of a NULL pointer prints (null), whole or cut by the precision, and a
 *    precision below its 6 characters prints nothing at all, which is what
 *    glibc does.
 *  - The sign of a NaN is not printed, so a negative one gives nan, where
 *    glibc gives -nan. An infinity keeps its sign.
 *  - The length modifier l is ignored by %s and %c, there are no wide
 *    characters. The strict mode rejects a length modifier on both of them,
 *    and on %s the flags the standard does not define there as well, and
 *    fails.
 *  - Not supported are the length modifier L (long double), the conversions
 *    a and A, and numbered arguments like %1$d. Only the % is printed:
 *    everything from it to the unsupported character goes with it, and the
 *    rest of the format is text, so %5k prints %, %La prints %a and %1$d
 *    prints %d. No argument is used, so a conversion after it reads the
 *    argument the unsupported one would have had. In the strict mode the
 *    function fails.
 * 
 *  With SNPRINTF_LEGACY_LENGTH the result of a truncated output is the number
 *  of characters written instead of the length of the whole output, and %n
 *  stores that number and is not reached at all when the output was already
 *  truncated, as before version 3.2. The strict mode is the exception, as it
 *  always processes the whole format: there %n is reached behind a full buffer
 *  and stores the number of characters written.
 * 
 * # Configuration macros
 * 
 *  Macro                           | Description
 * -------------------------------- | ----------------------------------------
 *  USE_SNPRINTF_PREFIX             | name the functions my_snprintf() and
 *                                  | my_vsnprintf(), and define snprintf and
 *                                  | vsnprintf as macros for them, to link
 *                                  | with the C library
 *  SNPRINTF_STRICT                 | return -1 for a malformed or unsupported
 *                                  | specifier, see above
 *  SNPRINTF_LEGACY_LENGTH          | return and store the number of characters
 *                                  | written into the buffer instead of the
 *                                  | length of the whole output, see above
 *  SNPRINTF_USE_MATH               | use modf() and signbit() of math.h
 *  SNPRINTF_FLOAT_INTEGRAL_DIGITS  | digits of the integral part of a double,
 *                                  | 309 by default, a smaller number saves
 *                                  | stack but bigger numbers are printed as
 *                                  | a row of nines, set it for snprintf.c
 *  SNPRINTF_FLOAT_PRECISION        | digits of the precision of %e, %E, %f,
 *                                  | %F, %g and %G, 29 by default. The C
 *                                  | standard asks for at least 999, so set
 *                                  | this higher, e.g. to 999, at the cost of
 *                                  | the stack, which grows by about 3 bytes
 *                                  | per digit for %e and %g and by about 1
 *                                  | for %f. A bigger precision is lowered to
 *                                  | this one, or fails in the strict mode,
 *                                  | set it for snprintf.c
 * 
 * @param string Output buffer, or NULL to calculate the length of the output.
 * @param length Size of the output buffer @p string, including the terminating
 *               '\0'. At most @p length - 1 characters are written. A length of
 *               0 measures the output without writing it.
 * @param format Format of input parameters.
 * @param ... Input parameters according of @p format.
 * 
 * @retval >=0 Number of characters of the whole output, not counting the
 *             terminating '\0', also when the output is truncated. An output
 *             longer than INT_MAX is reported as INT_MAX. With
 *             SNPRINTF_LEGACY_LENGTH it is the number of characters written,
 *             that is @p length - 1, for a truncated output.
 * @retval  -1 The strict mode does not accept the @p format.
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
