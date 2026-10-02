// Copyright (C) 2019-2026 Miroslaw Toton, mirtoto@gmail.com

/**
 * Portable snprintf() implementation.
 * @version 3.1
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * Revision History:
 *
 * @version 3.1
 * @author Miroslaw Toton (mirtoto), mirtoto@gmail.com
 *  - Safer %n: Honors hh, h, l, and ll pointer types, preventing 
 *    narrow-pointer overwrites.
 *  - Safer %s: Uses size_t for source lengths, bounds precision-limited scans, 
 *    and copies only bytes that fit.
 *  - Bounded formatting work: Numeric parsing saturates instead of overflowing, 
 *    and padding work is bounded by available output capacity.
 *  - Floating-point edge handling: Avoids subnormal exponent underflow and 
 *    handles NaN/Inf without entering decimal-conversion loops.
 *  - Expanded tests: Adds coverage for %n canaries, non-NUL-terminated 
 *    precision-limited strings, INT_MAX widths, subnormal doubles, 
 *    and integer conversion/precision combinations.
 * 
 * @version 3.0
 * @author Miroslaw Toton (mirtoto), mirtoto@gmail.com
 *  - Closer printf compatibility: Corrected sign and alignment flag handling, 
 *    zero-padding order, alternate octal zero, negative dynamic widths, 
 *    and %g significant-digit precision and notation boundaries.
 *  - Opt-in strict validation: SNPRINTF_STRICT rejects malformed formats and 
 *    unsupported flags, including malformed format tails after output 
 *    truncation. The default remains permissive.
 *  - Safer format processing: Width and precision parsing now saturates 
 *    instead of overflowing. Negative dynamic precision and INT_MIN width 
 *    are handled safely, and padding uses bounded bulk writes instead of 
 *    potentially huge per-character loops.
 *  - More robust floating-point output: Added NaN/Inf handling and fixes for 
 *    exponent carry, tiny values, high precision, field widths, and buffer 
 *    boundaries.
 *  - Optional math backend: The default floating-point implementation remains 
 *    self-contained. SNPRINTF_USE_MATH opts into <math.h> functions; 
 *    some toolchains require -lm.
 *  - Documentation and regression tests: Updated the README and public-header 
 *    documentation; expanded tests across integer, string, parser, 
 *    and floating-point edge cases.
 * 
 * @version 2.3
 * @author Miroslaw Toton (mirtoto), mirtoto@gmail.com
 *  - support NULL as output buffer to calculate size of output string
 *  - fix 0 precision for 0 value integers
 * 
 * @version 2.2
 * @author Miroslaw Toton (mirtoto), mirtoto@gmail.com
 *  - fix precision for integers
 *  - remove global buffers and unnecessary copying & lookin through srings
 *  - cleanup code and add Doxygen style comments
 * 
 * @version 2.1
 * @author Miroslaw Toton (mirtoto), mirtoto@gmail.com
 *  - fix problem with very big and very low "long long" values
 *  - change exponent width from 3 to 2
 *  - fix zero value for floating
 *  - support for "p" (%p)
 *
 * @version 2.0
 * @author Miroslaw Toton (mirtoto), mirtoto@gmail.com
 *  - move all defines & macros from header to codefile
 *  - support for "long long" (%llu, %lld, %llo, %llx)
 *  - fix for interpreting precision in some situations
 *  - fix unsigned (%u) for negative input
 *  - fix h & hh length of input number specifier
 *  - fix Clang linter warnings
 *
 * @version 1.1
 * @author Alain Magloire, alainm@rcsm.ee.mcgill.ca
 *  - added changes from Miles Bader
 *  - corrected a bug with %f
 *  - added support for %#g
 *  - added more comments :-)
 *
 * @version 1.0
 * @author Alain Magloire, alainm@rcsm.ee.mcgill.ca
 *  - supporting must ANSI syntaxic_sugars (see below)
 *
 * @version 0.0
 * @author Alain Magloire, alainm@rcsm.ee.mcgill.ca
 *  - suppot %s %c %d
 *
 * Floating-point conversion is self-contained, so it works on targets without
 * <math.h> or libm. Define SNPRINTF_USE_MATH to use modf() and signbit() from
 * math.h when the header and library are available.
 *
 * Points:
 *  - the value is split exactly into its integral and fractional parts;
 *  - the integral part is converted with integer arithmetic;
 *  - the fractional part is a fixed point big number, and every digit is the
 *    carry out of multiplying it by ten;
 *  - %f, %e and %g all use these digits and round to nearest, ties to even,
 *    so the output is the same as the C library gives for IEEE-754 doubles;
 *  - the precision is limited to MAX_PRECISION digits.
 */

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#ifdef SNPRINTF_USE_MATH
#include <math.h>
#endif

#include "snprintf.h"


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif


/** 
 * This struct holds temporary data during processing @p format 
 * of vsnprintf()/snprintf() functions.
 */
struct DATA {
  size_t counter;             /**< counter of length of string in DATA::ps */
  size_t ps_size;             /**< size of DATA::ps - 1 */
  char *ps;                   /**< pointer to output string */
  const char *pf;             /**< pointer to input format string */

/** Value of DATA::width - undefined width of field. */
#define WIDTH_UNSET          -1

  int width;                  /**< width of field */

/** Value of DATA::precision - undefined precision of field. */
#define PRECISION_UNSET      -1

  int precision;

/** Value of DATA::align - undefined align of field. */
#define ALIGN_UNSET           0
/** Value of DATA::align - align right of field. */
#define ALIGN_RIGHT           1
/** Value of DATA::align - align left of field. */
#define ALIGN_LEFT            2

  unsigned int align:2;     /**< align of field */
  unsigned int is_square:1; /**< is field with hash flag? */
  unsigned int is_plus:1;   /**< is field with plus sign flag? */
  unsigned int is_space:1;  /**< is field with space flag? */
  unsigned int is_dot:1;    /**< is field with dot flag? */
  unsigned int is_star_w:1; /**< is field with width defined? */
  unsigned int is_star_p:1; /**< is field with precision defined? */

/** Value of DATA::a_long - "int" type of input argument. */
#define INT_LEN_DEFAULT       0
/** Value of DATA::a_long - "long" type of input argument. */
#define INT_LEN_LONG          1
/** Value of DATA::a_long - "long long" of input type argument. */
#define INT_LEN_LONG_LONG     2
/** Value of DATA::a_long - "short" type of input argument. */
#define INT_LEN_SHORT         3
/** Value of DATA::a_long - "char" type of input argument. */
#define INT_LEN_CHAR          4

  unsigned int a_long:3;    /**< type of input */

  unsigned int rfu:6;       /**< RFU */

  char pad;                 /**< padding character */

  char slop[5];             /**< RFU */
};

/**
 * Put a @p c character to output buffer if there is enough space.
 * The @p c is evaluated once, even when there is no space or no buffer, so it
 * can have side effects, like PUT_CHAR(*text++, p).
 */
#define PUT_CHAR(c, p)                                  \
  do {                                                  \
    char put_char_value = (char)(c);                    \
    if ((p)->counter < (p)->ps_size) {                  \
      if ((p)->ps != NULL) {                            \
        *(p)->ps++ = put_char_value;                    \
      }                                                 \
      (p)->counter++;                                   \
    }                                                   \
  } while (0)

#define PUT_REPEAT(c, p, count)                         \
  do {                                                  \
    size_t repeat_count = (count) > 0 ? (size_t)(count) : 0; \
    size_t available = (p)->ps_size - (p)->counter;     \
    if (repeat_count > available) {                     \
      repeat_count = available;                         \
    }                                                   \
    if ((p)->ps != NULL && repeat_count > 0) {           \
      memset((p)->ps, (unsigned char)(c), repeat_count);\
      (p)->ps += repeat_count;                          \
    }                                                   \
    (p)->counter += repeat_count;                       \
  } while (0)

/** Put an optional '+' sign in the output buffer when the flag is set. */
#define PUT_PLUS(positive, p)                           \
  if ((positive) && (p)->is_plus) {                     \
    PUT_CHAR('+', p);                                   \
  }

/** Put an optional leading space if the number is positive and the flag is set. */
#define PUT_SPACE(positive, p)                          \
  if ((p)->is_space && !(p)->is_plus && (positive)) {   \
    PUT_CHAR(' ', p);                                   \
  }

/** Padding right optionally. */
#define PAD_RIGHT(p)                                    \
  do {                                                  \
    if ((p)->width > 0 && (p)->align != ALIGN_LEFT) {   \
      PUT_REPEAT((p)->pad, p, (p)->width);              \
      (p)->width = 0;                                   \
    }                                                   \
  } while (0)

/** Padding left optionally. */
#define PAD_LEFT(p)                                     \
  do {                                                  \
    if ((p)->width > 0 && (p)->align == ALIGN_LEFT) {   \
      PUT_REPEAT(' ', p, (p)->width);              \
      (p)->width = 0;                                   \
    }                                                   \
  } while (0)

/** Get width and precision arguments if available. */
#define WIDTH_AND_PRECISION_ARGS(p)                     \
  if ((p)->is_star_w) {                                 \
    int width_arg = va_arg(args, int);                  \
    if (width_arg < 0) {                                \
      (p)->align = ALIGN_LEFT;                           \
      (p)->width = width_arg == INT_MIN ? INT_MAX : -width_arg; \
    } else {                                            \
      (p)->width = width_arg;                           \
    }                                                   \
  }                                                     \
  if ((p)->is_star_p) {                                 \
    int precision_arg = va_arg(args, int);              \
    (p)->precision = precision_arg < 0 ? PRECISION_UNSET : precision_arg; \
  }

/** Get integer argument of given type and convert it to long long. */
#define INTEGER_ARG(p, type, ll)                        \
  WIDTH_AND_PRECISION_ARGS(p);                          \
  if ((p)->a_long == INT_LEN_LONG_LONG) {               \
    ll = (long long)va_arg(args, type long long);       \
  } else if ((p)->a_long == INT_LEN_LONG) {             \
    ll = (long long)va_arg(args, type long);            \
  } else {                                              \
    type int a = va_arg(args, type int);                \
    if ((p)->a_long == INT_LEN_SHORT) {                 \
      ll = (type short)a;                               \
    } else if ((p)->a_long == INT_LEN_CHAR) {           \
      ll = (type char)a;                                \
    } else {                                            \
      ll = a;                                           \
    }                                                   \
  }

/** Get double argument. */
#define DOUBLE_ARG(p, d)                                \
  WIDTH_AND_PRECISION_ARGS(p);                          \
  if ((p)->precision == PRECISION_UNSET) {              \
    (p)->precision = 6;                                 \
  } else if ((p)->precision > MAX_PRECISION) {          \
    (p)->precision = MAX_PRECISION;                     \
  }                                                     \
  d = va_arg(args, double);

/**
 * Convert @p a string to @p res integer.
 * 
 * Function stop conversion and return result in any of the following cases:
 *  - encounter end of string ('\0') character,
 *  - encounter non digit character.
 * 
 * @param a Not NULL input string.
 * @param res Not NULL pointer to output integer.
 * 
 * @return Number of parsed character form @p a.
 */
static size_t strtoi(const char *a, int *res) {
  size_t i = 0;
  unsigned int value = 0;

  for (; a[i] != '\0' && isdigit((unsigned char)a[i]); i++) {
    unsigned int digit = (unsigned int)(a[i] - '0');
    if (value > ((unsigned int)INT_MAX - digit) / 10U) {
      value = INT_MAX;
    } else {
      value = value * 10U + digit;
    }
  }

  *res = (int)value;
  return i;
}

/**
 * Convert @p number to string representation of given @p base.
 *
 * @param number Input number to conversion.
 * @param is_signed Interpret @p number as 'unsigned' (0) / 'signed' (1).
 * @param precision Input @p number precision.
 * @param base Output base (8, 10, 16).
 * @param output Buffer for output string.
 * @param output_size Size of @p optput buffer (at least 3 characters).
 */
static void inttoa(long long number, int is_signed, int precision, int base,
    char *output, size_t output_size) {    
  size_t i = 0, j;

  output_size--; /* for '\0' character */

  if (number != 0) {
    unsigned long long n;

    if (is_signed && number < 0) {
      if (number == LLONG_MIN) {
        n = (unsigned long long)LLONG_MAX + 1ULL;
      } else {
        n = (unsigned long long)-number;
      }
      output_size--; /* for '-' character */
    } else {
      n = (unsigned long long)number;
    }

    while (n != 0 && i < output_size) {
      int r = (int)(n % (unsigned long long)(base));
      output[i++] = (char)r + (r < 10 ? '0' : 'a' - 10);
      n /= (unsigned long long)(base);
    }

    if (precision > 0) { /* precision defined ? */
      for (; i < (size_t)precision && i < output_size; i++) {
        output[i] = '0';
      }
    }

    /* put the sign ? */
    if (is_signed && number < 0) {
      output[i++] = '-';
    }

    output[i] = '\0';
    
    /* reverse every thing */
    for (i--, j = 0; j < i; j++, i--) {
      char tmp = output[i];
      output[i] = output[j];
      output[j] = tmp;
    }
  } else {
    precision = precision < 0 ? 1 : precision;
    for (i = 0; i < (size_t)precision && i < output_size; i++) {
      output[i] = '0';
    }
    output[i] = '\0';
  }
}

/**
 * Tell if @p d is printed with a minus sign. That is true for every negative
 * number and for negative zero too.
 */
static int has_minus(double d) {
  if (d != 0.) {
    return d < 0.;
  }
#ifdef SNPRINTF_USE_MATH
  return signbit(d) != 0;
#else
  return 1. / d < 0.; /* minus infinity for negative zero */
#endif
}

/**
 * Split @p real into its integral and fractional parts.
 *
 * The default implementation stays self-contained; SNPRINTF_USE_MATH uses
 * modf() for the split.
 */
static double integral(double real, double *ip) {
#ifdef SNPRINTF_USE_MATH
  if (real < 0.) {
    real = -real;
  }

  return modf(real, ip);
#else
  /* equal to zero ? */
  if (real == 0.) {
    *ip = 0.;
    return 0.;
  }

  /* negative number ? */
  if (real < 0.) {
    real = -real;
  }

  /* a fraction ? */
  if (real < 1.) {
    *ip = 0.;
    return real;
  }

  /* from 2^52 on every double is integral already */
  if (real >= 4503599627370496.) {
    *ip = real;
    return 0.;
  }

  /* the real work :-) */
  *ip = (double)(unsigned long long)real; /* truncation is exact */
  return (real - *ip);
#endif
}

/** Maximum size of the buffer for the digits of an integer. */
#define MAX_INTEGRAL_SIZE (99 + 1)

/**
 * Maximum number of digits of the integral part of a double. The biggest
 * finite double has 309 of them. A smaller value saves stack, but bigger
 * numbers are then printed as a row of nines.
 */
#ifndef SNPRINTF_FLOAT_INTEGRAL_DIGITS
#define SNPRINTF_FLOAT_INTEGRAL_DIGITS 309
#endif
/** Maximum size of the buffer for the integral part of a double. */
#define MAX_FLOAT_INTEGRAL_SIZE (SNPRINTF_FLOAT_INTEGRAL_DIGITS + 2)
/** Maximum precision of floating-point conversions, in digits. */
#define MAX_PRECISION 29
/** Maximum size of the buffer for the fraction part: %g needs 3 more digits. */
#define MAX_FRACTION_SIZE (MAX_PRECISION + 3 + 1)

/** Base of the limbs used by integer_limbs(): 9 decimal digits each. */
#define INTEGER_LIMB_BASE 1000000000UL
/** Number of limbs used by integer_limbs(): enough for the integral part. */
#define INTEGER_LIMBS ((SNPRINTF_FLOAT_INTEGRAL_DIGITS + 8) / 9)

/**
 * Convert the non-negative, integral @p value to base 10^9 limbs without any
 * loss of precision. The least significant limb comes first.
 *
 * Values below 2^64 are converted with integer arithmetic. Bigger values are
 * a 64-bit integer multiplied by a power of two (exact for a double), which
 * is done on the limbs, 30 bits at a time.
 *
 * @param value Finite value to convert.
 * @param limbs Array of INTEGER_LIMBS limbs.
 *
 * @return Number of limbs (at least 1), or 0 if @p value does not fit.
 */
static size_t integer_limbs(double value, uint32_t *limbs) {
  unsigned long long n;
  size_t used = 0, i;
  int shift = 0;

  while (value >= 18446744073709551616.) { /* 2^64 */
    value /= 2.; /* exact, only the exponent changes */
    shift++;
  }
  n = (unsigned long long)value;

  do {
    if (used == INTEGER_LIMBS) {
      return 0;
    }
    limbs[used++] = (uint32_t)(n % INTEGER_LIMB_BASE);
    n /= INTEGER_LIMB_BASE;
  } while (n != 0);

  while (shift > 0) { /* multiply by 2^bits, at most 2^30 to stay below 2^64 */
    int bits = shift < 30 ? shift : 30;
    unsigned long long carry = 0;
    for (i = 0; i < used; i++) {
      unsigned long long v = ((unsigned long long)limbs[i] << bits) + carry;
      limbs[i] = (uint32_t)(v % INTEGER_LIMB_BASE);
      carry = v / INTEGER_LIMB_BASE;
    }
    for (; carry != 0; carry /= INTEGER_LIMB_BASE) {
      if (used == INTEGER_LIMBS) {
        return 0;
      }
      limbs[used++] = (uint32_t)(carry % INTEGER_LIMB_BASE);
    }
    shift -= bits;
  }

  return used;
}

/**
 * Convert the non-negative, integral @p value to decimal digits without any
 * loss of precision. The digits are stored least significant first, without
 * a terminating '\0'.
 *
 * @return Number of digits (at least 1), or 0 if @p value needs more than
 *         @p size digits.
 */
static size_t integer_digits(double value, char *output, size_t size) {
  uint32_t limbs[INTEGER_LIMBS];
  size_t used = integer_limbs(value, limbs);
  size_t count = 0, i;

  if (used == 0) {
    return 0;
  }

  for (i = 0; i < used; i++) {
    uint32_t v = limbs[i];
    int digits;
    for (digits = 0; digits < 9; digits++) {
      if (i == used - 1 && v == 0) { /* no leading zeros */
        break;
      }
      if (count == size) {
        return 0;
      }
      output[count++] = (char)('0' + v % 10);
      v /= 10;
    }
  }

  if (count == 0) {
    output[count++] = '0';
  }

  return count;
}

/**
 * Get the first @p count + 1 digits of the non-negative, integral @p value,
 * most significant first, and tell if the digits after them are all zero.
 *
 * @param value Finite value to convert.
 * @param count Number of digits wanted, the next one is always added.
 * @param output Buffer for @p count + 1 digits, without a terminating '\0'.
 * @param total Number of digits of @p value.
 * @param sticky Set if a digit after the first @p count + 1 is not zero.
 *
 * @return 1 on success, 0 if @p value does not fit.
 */
static int integer_top(double value, size_t count, char *output,
    size_t *total, int *sticky) {
  uint32_t limbs[INTEGER_LIMBS];
  size_t used = integer_limbs(value, limbs);
  size_t taken = 0, i, d, lead = 1;
  uint32_t v;
  char text[9];

  if (used == 0) {
    return 0;
  }

  for (v = limbs[used - 1]; v >= 10; v /= 10) { /* digits of the first limb */
    lead++;
  }
  *total = 9 * (used - 1) + lead;
  *sticky = 0;

  for (i = used; i-- > 0 && taken <= count;) {
    v = limbs[i];
    for (d = 9; d-- > 0;) {
      text[d] = (char)('0' + v % 10);
      v /= 10;
    }
    for (d = i == used - 1 ? 9 - lead : 0; d < 9 && taken <= count; d++) {
      output[taken++] = text[d];
    }
    if (taken > count) { /* the rest of this limb and the limbs below it */
      for (; d < 9; d++) {
        if (text[d] != '0') {
          *sticky = 1;
        }
      }
      while (i-- > 0) {
        if (limbs[i] != 0) {
          *sticky = 1;
        }
      }
      break;
    }
  }

  return 1;
}

/** Number of 32-bit limbs of a fraction: 1152 bits hold any double fraction. */
#define FRACTION_LIMBS 36

/**
 * Fraction as a fixed point number with FRACTION_LIMBS * 32 fractional bits.
 *
 * A double is an integer times a power of two, so its fraction is exact in
 * this form, and multiplying it by ten gives its next decimal digit as the
 * carry out of the most significant limb.
 */
struct FRACTION {
  uint32_t limbs[FRACTION_LIMBS]; /**< bits of the fraction, least significant first */
  size_t first;                   /**< lowest limb that can be non-zero */
};

/** Set @p f to @p value, where 0 <= @p value < 1. */
static void fraction_init(struct FRACTION *f, double value) {
  unsigned long long mantissa;
  uint32_t low, high;
  size_t i, words;
  int scale = 52;
  int bits;

  for (i = 0; i < FRACTION_LIMBS; i++) {
    f->limbs[i] = 0;
  }
  f->first = FRACTION_LIMBS;
  if (value <= 0.) {
    return;
  }

  /* value = mantissa * 2^-scale, with 2^52 <= mantissa < 2^53 */
  while (value < 1.) {
    value *= 2.; /* exact, only the exponent changes */
    scale++;
  }
  mantissa = (unsigned long long)(value * 4503599627370496.); /* 2^52 */

  /* limbs = mantissa * 2^(FRACTION_LIMBS * 32 - scale) */
  words = (size_t)(FRACTION_LIMBS * 32 - scale) / 32;
  bits = (FRACTION_LIMBS * 32 - scale) % 32;
  low = (uint32_t)(mantissa & 0xffffffffUL);
  high = (uint32_t)(mantissa >> 32);
  f->limbs[words] = low << bits;
  if (words + 1 < FRACTION_LIMBS) {
    f->limbs[words + 1] = (high << bits) | (bits != 0 ? low >> (32 - bits) : 0);
  }
  if (words + 2 < FRACTION_LIMBS && bits != 0) {
    f->limbs[words + 2] = high >> (32 - bits);
  }
  f->first = words;
}

/** Take the next decimal digit of @p f off the fraction. */
static int fraction_next(struct FRACTION *f) {
  unsigned long long carry = 0;
  size_t j;

  for (j = f->first; j < FRACTION_LIMBS; j++) { /* multiply by ten */
    unsigned long long v = (unsigned long long)f->limbs[j] * 10U + carry;
    f->limbs[j] = (uint32_t)v;
    carry = v >> 32;
  }
  while (f->first < FRACTION_LIMBS && f->limbs[f->first] == 0) {
    f->first++;
  }

  return (int)carry;
}

/**
 * Compare what is left of @p f with one half, that is 2^(FRACTION_LIMBS * 32 - 1).
 *
 * @retval 0 The rest is below one half.
 * @retval 1 The rest is exactly one half.
 * @retval 2 The rest is above one half.
 */
static int fraction_rest(const struct FRACTION *f) {
  size_t j;

  if (f->limbs[FRACTION_LIMBS - 1] != 0x80000000UL) {
    return f->limbs[FRACTION_LIMBS - 1] > 0x80000000UL ? 2 : 0;
  }
  for (j = f->first; j < FRACTION_LIMBS - 1; j++) {
    if (f->limbs[j] != 0) {
      return 2;
    }
  }

  return 1;
}

/**
 * Generate @p count decimal digits of the fraction @p value, without any loss
 * of precision, and tell how the rest of the fraction compares to one half.
 *
 * @param value Fraction to convert, 0 <= @p value < 1.
 * @param count Number of digits to generate.
 * @param output Buffer for @p count digits, without a terminating '\0'.
 *
 * @return The result of fraction_rest(): 0, 1 or 2.
 */
static int fraction_digits(double value, size_t count, char *output) {
  struct FRACTION f;
  size_t i;

  fraction_init(&f, value);
  for (i = 0; i < count; i++) {
    output[i] = (char)('0' + fraction_next(&f));
  }

  return fraction_rest(&f);
}

/**
 * Return an ASCII representation of the integral and fraction
 * part of the @p number.
 *
 * The digits are those of the exact binary value of @p number, rounded to
 * @p precision digits to nearest, ties to even, like the C library does.
 */
static void floattoa(double number, int precision,
    char *output_integral, size_t output_integral_size,
    char *output_fraction, size_t output_fraction_size) {

  size_t i, j;
  size_t digits;
  int is_negative = 0;
  int rest;
  double ip;
  double fraction;

  /* for negative numbers */
  if (has_minus(number)) {
    number = -number;
    is_negative = 1;
    output_integral_size--; /* sign consume one digit */
  }

  fraction = integral(number, &ip);
  /* do the integral part */
  i = integer_digits(ip, output_integral, output_integral_size - 1);

  /* Oh No !! out of bound, ho well fill it up ! */
  if (i == 0) {
    for (i = 0; i < output_integral_size - 1; ++i) {
      output_integral[i] = '9';
    }
  }

  /* the fractional part */
  digits = precision > 0 ? (size_t)precision : 0;
  if (digits > output_fraction_size - 1) {
    digits = output_fraction_size - 1;
  }
  rest = fraction_digits(fraction, digits, output_fraction);
  output_fraction[digits] = '\0';

  /* round to nearest, ties to even */
  if (rest == 2 || (rest == 1 &&
      (digits > 0 ? output_fraction[digits - 1] : output_integral[0]) % 2 != 0)) {
    int carry = 1;
    for (j = digits; carry && j > 0; j--) {
      if (output_fraction[j - 1] == '9') {
        output_fraction[j - 1] = '0';
      } else {
        output_fraction[j - 1]++;
        carry = 0;
      }
    }
    for (j = 0; carry && j < i; j++) {
      if (output_integral[j] == '9') {
        output_integral[j] = '0';
      } else {
        output_integral[j]++;
        carry = 0;
      }
    }
    if (carry) {
      if (i < output_integral_size - 1) {
        output_integral[i++] = '1';
      } else { /* no room for one more digit, stay at the biggest value */
        for (j = 0; j < i; j++) {
          output_integral[j] = '9';
        }
      }
    }
  }

  /* put the sign ? */
  if (is_negative) {
    output_integral[i++] = '-';
  }

  output_integral[i] = '\0';

  /* reverse every thing */
  for (i--, j = 0; j < i; j++, i--) {
    char tmp = output_integral[i];
    output_integral[i] = output_integral[j];
    output_integral[j] = tmp;
  }
}

/**
 * Convert the positive, finite @p value to @p count significant decimal
 * digits, rounded to nearest, ties to even, like the C library does.
 *
 * @param value Value to convert, greater than zero.
 * @param count Number of digits, at most MAX_PRECISION + 1.
 * @param digits Buffer for @p count digits, without a terminating '\0'.
 * @param exp10 Decimal exponent of the first digit, after rounding.
 */
static void significant_digits(double value, size_t count, char *digits,
    int *exp10) {
  char top[MAX_PRECISION + 3];
  double ip;
  double fraction = integral(value, &ip);
  size_t i, n = 0;
  int rest;

  if (ip >= 1.) { /* the digits come from the integral part first */
    int sticky = 0;
    if (!integer_top(ip, count, top, &n, &sticky)) {
      for (i = 0; i < count; i++) { /* out of bound, fill it up ! */
        digits[i] = '9';
      }
      *exp10 = SNPRINTF_FLOAT_INTEGRAL_DIGITS - 1;
      return;
    }
    *exp10 = (int)n - 1;
    if (n > count) {
      int next = top[count] - '0';
      memcpy(digits, top, count);
      rest = next > 5 ? 2 : next < 5 ? 0 : (sticky || fraction != 0.) ? 2 : 1;
    } else {
      memcpy(digits, top, n);
      if (n < count) {
        rest = fraction_digits(fraction, count - n, digits + n);
      } else {
        rest = fraction > .5 ? 2 : fraction == .5 ? 1 : 0;
      }
    }
  } else { /* the digits come from the fraction, after some zeros */
    struct FRACTION f;
    int digit = 0, zeros = 0;
    fraction_init(&f, fraction);
    while (zeros <= 330 && (digit = fraction_next(&f)) == 0) {
      zeros++;
    }
    *exp10 = -(zeros + 1);
    digits[0] = (char)('0' + digit);
    for (i = 1; i < count; i++) {
      digits[i] = (char)('0' + fraction_next(&f));
    }
    rest = fraction_rest(&f);
  }

  /* round to nearest, ties to even */
  if (rest == 2 || (rest == 1 && (digits[count - 1] - '0') % 2 != 0)) {
    for (i = count; i > 0 && digits[i - 1] == '9'; i--) {
      digits[i - 1] = '0';
    }
    if (i > 0) {
      digits[i - 1]++;
    } else { /* all nines became zeros, 9.99 -> 10.0 */
      digits[0] = '1';
      (*exp10)++;
    }
  }
}

/**
 * Get the decimal exponent of @p d in scientific notation, after @p d is
 * rounded to @p precision significant digits. It is 0 for zero.
 */
static int rounded_exponent(double d, int precision) {
  char digits[MAX_PRECISION + 2];
  int exp10 = 0;

  if (d != 0.) {
    significant_digits(d < 0. ? -d : d, (size_t)precision, digits, &exp10);
  }

  return exp10;
}

/** Emit a sign prefix before zero-filled numeric output. */
static void emit_sign_prefix(struct DATA *p, int is_negative) {
  if (is_negative) {
    PUT_CHAR('-', p);
  } else if (p->is_plus) {
    PUT_CHAR('+', p);
  } else if (p->is_space) {
    PUT_CHAR(' ', p);
  }
}

/** Emit a format prefix before zero-filled numeric output. */
static void emit_format_prefix(struct DATA *p, const char *prefix) {
  if (prefix != NULL) {
    while (*prefix != '\0') {
      PUT_CHAR(*prefix++, p);
    }
  }
}

/** Format @p ll number as ASCII decimal string according to @p p flags. */
static void decimal(struct DATA *p, long long ll) {
  char number[MAX_INTEGRAL_SIZE];
  const char *digits = number;
  const int is_signed = *p->pf == 'i' || *p->pf == 'd';
  const int is_negative = is_signed && ll < 0;
  int sign = 0;

  inttoa(ll, is_signed, p->precision, 10, number, sizeof(number));

  if (p->precision >= 0) { /* the '0' flag is ignored when precision is given */
    p->pad = ' ';
  }

  if (is_negative) {
    digits = number + 1;
    sign = 1;
  } else if (is_signed && (p->is_plus || p->is_space)) {
    sign = 1; /* '+' and ' ' apply to signed conversions only */
  }

  p->width -= (int)strlen(digits) + sign;
  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    if (sign) {
      emit_sign_prefix(p, is_negative);
    }
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    if (sign) {
      emit_sign_prefix(p, is_negative);
    }
  }

  for (; *digits != '\0'; digits++) {
    PUT_CHAR(*digits, p);
  }

  PAD_LEFT(p);
}

/** Format @p ll number as ASCII octal string according to @p p flags. */
static void octal(struct DATA *p, long long ll) {
  char number[MAX_INTEGRAL_SIZE], *pnumber = number;
  const char *prefix = NULL;

  inttoa(ll, 0, p->precision, 8, number, sizeof(number));

  if (p->precision >= 0) { /* the '0' flag is ignored when precision is given */
    p->pad = ' ';
  }

  if (p->is_square) {
    if (*number == '\0') {
      number[0] = '0';
      number[1] = '\0';
    } else if (*number != '0') {
      prefix = "0";
    }
  }

  p->width -= (int)strlen(number) + (prefix != NULL ? 1 : 0);
  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    emit_format_prefix(p, prefix);
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    emit_format_prefix(p, prefix);
  }

  for (; *pnumber != '\0'; pnumber++) {
    PUT_CHAR(*pnumber, p);
  }

  PAD_LEFT(p);
}

/** Format @p ll number as ASCII hexadecimal string according to @p p flags. */
static void hex(struct DATA *p, long long ll) {
  char number[MAX_INTEGRAL_SIZE], *pnumber = number;
  const char *prefix = NULL;

  inttoa(ll, 0, p->precision, 16, number, sizeof(number));

  if (p->precision >= 0) { /* the '0' flag is ignored when precision is given */
    p->pad = ' ';
  }

  if (p->is_square && ll != 0) { /* no "0x" prefix for a zero value */
    prefix = *p->pf == 'p' ? "0x" : (*p->pf == 'X' ? "0X" : "0x");
  }

  p->width -= (int)strlen(number) + (prefix != NULL ? (int)strlen(prefix) : 0);
  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    emit_format_prefix(p, prefix);
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    emit_format_prefix(p, prefix);
  }

  for (; *pnumber != '\0'; pnumber++) {
    PUT_CHAR((*p->pf == 'X' ? (char)toupper(*pnumber) : *pnumber), p);
  }

  PAD_LEFT(p);
}

/** Format @p str string according to @p p flags. */
static void strings(struct DATA *p, const char *s) {
  const char *src = s == NULL ? "(null)" : s;
  size_t len = 0;
  size_t padding = 0;
  size_t available;
  size_t copy_length;

  p->pad = ' '; /* the '0' flag is undefined for strings; libc pads with blanks */

  if (p->precision >= 0) {
    while (len < (size_t)p->precision && src[len] != '\0') {
      len++;
    }
  } else {
    len = strlen(src);
  }

  if (p->width > 0 && len < (size_t)p->width) {
    padding = (size_t)p->width - len;
  }
  p->width = (int)padding;

  PAD_RIGHT(p);

  available = p->ps_size - p->counter;
  copy_length = len < available ? len : available;
  if (p->ps != NULL && copy_length > 0) {
    memcpy(p->ps, src, copy_length);
    p->ps += copy_length;
  }
  p->counter += copy_length;

  PAD_LEFT(p);
}

static int special_float(struct DATA *p, double value) {
  int is_nan = value != value;
  int is_negative = value < 0.;
  int is_upper = *p->pf == 'F' || *p->pf == 'E' || *p->pf == 'G';
  const char *text;
  char pad = p->pad;

  if (is_nan) {
    text = is_upper ? "NAN" : "nan";
  } else if (value > DBL_MAX || value < -DBL_MAX) {
    text = is_upper ? "INF" : "inf";
  } else {
    return 0;
  }

  p->width -= (int)strlen(text) +
      (is_negative || p->is_plus || p->is_space ? 1 : 0);
  p->pad = ' ';
  PAD_RIGHT(p);

  if (is_negative) {
    PUT_CHAR('-', p);
  } else if (p->is_plus) {
    PUT_CHAR('+', p);
  } else if (p->is_space) {
    PUT_CHAR(' ', p);
  }

  while (*text != '\0') {
    PUT_CHAR(*text++, p);
  }

  PAD_LEFT(p);
  p->pad = pad;
  return 1;
}

/** 
 * Format @p d floating point number as ASCII decimal floating point 
 * according to @p p flags.
 */
static void floating(struct DATA *p, double d) {
  char integral[MAX_FLOAT_INTEGRAL_SIZE], *pintegral = integral;
  char fraction[MAX_FRACTION_SIZE], *pfraction = fraction;
  int is_general = *p->pf == 'g' || *p->pf == 'G';
  int minus = has_minus(d);
  int has_dot;

  floattoa(d, p->precision,
    integral, sizeof(integral), fraction, sizeof(fraction));

  if (is_general && !p->is_square) {
    size_t i;
    for (i = strlen(fraction); i > 0 && fraction[i - 1] == '0'; i--) {
      fraction[i - 1] = '\0';
    }
  }
  has_dot = p->is_square || (p->precision != 0 &&
      (!is_general || fraction[0] != '\0'));
  p->width -= (int)strlen(integral) + (int)strlen(fraction) + has_dot;
  if (!minus && (p->is_plus || p->is_space)) {
    p->width -= 1;
  }
  
  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    if (*pintegral == '-') {
      PUT_CHAR(*pintegral++, p);
    } else {
      PUT_PLUS(!minus, p);
      PUT_SPACE(!minus, p);
    }
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    PUT_PLUS(!minus, p);
    PUT_SPACE(!minus, p);
  }

  for (; *pintegral != '\0'; pintegral++) {
    PUT_CHAR(*pintegral, p);
  }

  if (has_dot) { /* put the '.' */
    PUT_CHAR('.', p);
  }

  for (; *pfraction != '\0'; pfraction++) {
    PUT_CHAR(*pfraction, p);
  }

  PAD_LEFT(p);
}

/** 
 * Format @p d floating point number as ASCII scientific (exponential)
 * floating point according to @p p flags.
 */
static void exponent(struct DATA *p, double d) {
  char integral[MAX_INTEGRAL_SIZE], *pintegral = integral;
  char fraction[MAX_FRACTION_SIZE], *pfraction = fraction;
  char digits[MAX_PRECISION + 2];
  int log = 0;
  int is_general = *p->pf == 'g' || *p->pf == 'G';
  int minus = has_minus(d);
  int has_dot;
  int exponent_digits;
  size_t k;

  /* the first digit, then precision digits more */
  if (d == 0.) {
    for (k = 0; k <= (size_t)p->precision; k++) {
      digits[k] = '0';
    }
  } else {
    significant_digits(minus ? -d : d, (size_t)p->precision + 1, digits, &log);
  }
  k = 0;
  if (minus) {
    integral[k++] = '-';
  }
  integral[k++] = digits[0];
  integral[k] = '\0';
  memcpy(fraction, digits + 1, (size_t)p->precision);
  fraction[p->precision] = '\0';

  if (is_general && !p->is_square) {
    size_t i;
    for (i = strlen(fraction); i > 0 && fraction[i - 1] == '0'; i--) {
      fraction[i - 1] = '\0';
    }
  }
  has_dot = p->is_square || (p->precision != 0 &&
      (!is_general || fraction[0] != '\0'));
  exponent_digits = log <= -100 || log >= 100 ? 3 : 2;
  p->width -= (int)strlen(integral) + (int)strlen(fraction) + has_dot +
      exponent_digits + 2;
  if (!minus && (p->is_plus || p->is_space)) {
    p->width -= 1;
  }

  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    if (*pintegral == '-') {
      PUT_CHAR(*pintegral++, p);
    } else {
      PUT_PLUS(!minus, p);
      PUT_SPACE(!minus, p);
    }
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    PUT_PLUS(!minus, p);
    PUT_SPACE(!minus, p);
  }

  for (; *pintegral != '\0'; pintegral++) {
    PUT_CHAR(*pintegral, p);
  }

  if (has_dot) { /* the '.' */
    PUT_CHAR('.', p);
  }
  for (; *pfraction != '\0'; pfraction++) {
    PUT_CHAR(*pfraction, p);
  }

  if (*p->pf == 'g' || *p->pf == 'e') { /* the exponent put the 'e|E' */
    PUT_CHAR('e', p);
  } else {
    PUT_CHAR('E', p);
  }

  if (log >= 0) { /* the sign of the exp */
    PUT_CHAR('+', p);
  }

  inttoa(log, 1, 2, 10, integral, sizeof(integral));
  for (pintegral = integral; *pintegral != '\0'; pintegral++) { /* exponent */
    PUT_CHAR(*pintegral, p);
  }

  PAD_LEFT(p);
}

/** Initialize and parse the conversion specifiers. */
static void conv_flags(struct DATA *p) {
  p->width = WIDTH_UNSET;
  p->precision = PRECISION_UNSET;
  p->is_star_w = p->is_star_p = 0;
  p->is_square = p->is_plus = p->is_space = 0;
  p->a_long = INT_LEN_DEFAULT;
  p->align = ALIGN_UNSET;
  p->pad = ' ';
  p->is_dot = 0;

  for (; p != NULL && p->pf != NULL; p->pf++) {
    switch (*p->pf) {
      case ' ':
        p->is_space = 1;
        break;

      case '#':
        p->is_square = 1;
        break;

      case '*':
        if (p->width == WIDTH_UNSET) {
          p->width = 1;
          p->is_star_w = 1;
        } else {
          p->precision = 1;
          p->is_star_p = 1;
        }
        break;

      case '+':
        p->is_plus = 1;
        break;

      case '-':
        p->align = ALIGN_LEFT;
        break;

      case '.':
        if (p->width == WIDTH_UNSET) {
          p->width = 0;
        }
        p->is_dot = 1;
        p->precision = 0; /* a lone '.' means precision 0 */
        break;

      case '0':
        if (p->is_dot) {
          p->precision = 0;
        } else {
          p->pad = '0';
        }
        break;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': /* get all the digits */
        p->pf += strtoi(p->pf,
          p->width == WIDTH_UNSET ? &p->width : &p->precision) - 1;
        break;

      case '%':
        return;

      default:
        p->pf--; /* Reprocess this character as the conversion specifier. */
        return;
    }
  }
}

int SNPRINTF_PREFIX(vsnprintf)(char *string, size_t length, const char *format, va_list args) {
  struct DATA data;

  /* Count the required output length without writing to a buffer. */
  if (string == NULL) {
    length = __SIZE_MAX__;
  /* A non-NULL output buffer must have nonzero capacity. */
  } else if (length < 1) {
    return -1;
  }

  data.ps_size = length - 1; /* leave room for '\0' */
  data.ps = string;
  data.pf = format;
  data.counter = 0;

  for (; *data.pf != '\0'; data.pf++) {
#ifndef SNPRINTF_STRICT
    if (data.counter >= data.ps_size) {
      break;
    }
#endif
    if (*data.pf == '%') { /* Start parsing a conversion specifier. */
      int is_continue = 1;
      conv_flags(&data); /* initialise format flags */
      while (*data.pf != '\0' && is_continue) {
        switch (*(++data.pf)) {
          case '\0': /* The format string ended before a conversion specifier. */
#ifdef SNPRINTF_STRICT
            return -1;
#endif
            PUT_CHAR('%', &data);
            if (data.ps != NULL) {
              *data.ps = '\0';
            }
            return (int)data.counter;

          case 'f':
          case 'F': { /* decimal floating point */
            double d;
            DOUBLE_ARG(&data, d);
            if (!special_float(&data, d)) {
              floating(&data, d);
            }
            is_continue = 0;
            break;
          }

          case 'e':
          case 'E': { /* scientific (exponential) floating point */
            double d;
            DOUBLE_ARG(&data, d);
            if (!special_float(&data, d)) {
              exponent(&data, d);
            }
            is_continue = 0;
            break;
          }

          case 'g':
          case 'G': { /* scientific or decimal floating point */
            int log;
            double d;
            DOUBLE_ARG(&data, d);
            if (special_float(&data, d)) {
              is_continue = 0;
              break;
            }
            if (data.precision < 0) {
              data.precision = 6;
            } else if (data.precision == 0) {
              data.precision = 1;
            }
            log = rounded_exponent(d, data.precision);
            /* use decimal floating point (%f / %F) if exponent is in the range
               [-4,precision] exclusively else use scientific floating
               point (%e / %E) */
            if (-4 <= log && log < data.precision) {
              data.precision -= log + 1;
              floating(&data, d);
            } else {
              data.precision--;
              exponent(&data, d);
            }
            is_continue = 0;
            break;
          }

          case 'u': { /* unsigned decimal integer */
            long long ll;
            INTEGER_ARG(&data, unsigned, ll);
            decimal(&data, ll);
            is_continue = 0;
            break;
          }

          case 'i':
          case 'd': { /* signed decimal integer */
            long long ll;
            INTEGER_ARG(&data, signed, ll);
            decimal(&data, ll);
            is_continue = 0;
            break;
          }

          case 'o': { /* octal (always unsigned) */
            long long ll;
            INTEGER_ARG(&data, unsigned, ll);
            octal(&data, ll);
            is_continue = 0;
            break;
          }

          case 'x':
          case 'X': { /* hexadecimal (always unsigned) */
            long long ll;
            INTEGER_ARG(&data, unsigned, ll);
            hex(&data, ll);
            is_continue = 0;
            break;
          }

          case 'c': { /* single character */
            int i;
            char pad = data.pad;
            WIDTH_AND_PRECISION_ARGS(&data);
            i = va_arg(args, int);
            data.width--;
            data.pad = ' ';
            PAD_RIGHT(&data);
            PUT_CHAR((char)i, &data);
            PAD_LEFT(&data);
            data.pad = pad;
            is_continue = 0;
            break;
          }

          case 's': /* string of characters */
#ifdef SNPRINTF_STRICT
            if (data.is_square || data.is_plus || data.is_space ||
                data.pad == '0' || data.a_long != INT_LEN_DEFAULT) {
              return -1;
            }
#endif
            WIDTH_AND_PRECISION_ARGS(&data);
            strings(&data, va_arg(args, char *));
            is_continue = 0;
            break;

          case 'p': { /* pointer */
            WIDTH_AND_PRECISION_ARGS(&data);
            void *v = va_arg(args, void *);
            data.is_square = 1;
            if (v == NULL) {
              strings(&data, "(nil)");
            } else {
              hex(&data, (long long)(uintptr_t)v); /* no sign extension */
            }
            is_continue = 0;
            break;
          }

          case 'n': /* Store the output count using the requested integer type. */
            switch (data.a_long) {
              case INT_LEN_CHAR:
                *va_arg(args, signed char *) = (signed char)data.counter;
                break;
              case INT_LEN_SHORT:
                *va_arg(args, short *) = (short)data.counter;
                break;
              case INT_LEN_LONG:
                *va_arg(args, long *) = (long)data.counter;
                break;
              case INT_LEN_LONG_LONG:
                *va_arg(args, long long *) = (long long)data.counter;
                break;
              default:
                *va_arg(args, int *) = (int)data.counter;
                break;
            }
            is_continue = 0;
            break;

          case 'l': /* long or long long */
            if (data.a_long == INT_LEN_LONG) {
              data.a_long = INT_LEN_LONG_LONG;
            } else {
              data.a_long = INT_LEN_LONG;
            }
            break;

          case 'h': /* short or char */
            if (data.a_long == INT_LEN_SHORT) {
              data.a_long = INT_LEN_CHAR;
            } else {
              data.a_long = INT_LEN_SHORT;
            }
            break;

          case 'z': /* size_t / ssize_t */
            data.a_long = sizeof(size_t) <= sizeof(int) ? INT_LEN_DEFAULT :
              sizeof(size_t) <= sizeof(long) ? INT_LEN_LONG : INT_LEN_LONG_LONG;
            break;

          case 't': /* ptrdiff_t */
            data.a_long = sizeof(ptrdiff_t) <= sizeof(int) ? INT_LEN_DEFAULT :
              sizeof(ptrdiff_t) <= sizeof(long) ? INT_LEN_LONG : INT_LEN_LONG_LONG;
            break;

          case 'j': /* intmax_t / uintmax_t */
            data.a_long = INT_LEN_LONG_LONG;
            break;

          case '%': /* nothing just % */
            PUT_CHAR('%', &data);
            is_continue = 0;
            break;

          case '#':
          case ' ':
          case '+':
          case '*':
          case '-':
          case '.':
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            conv_flags(&data);
            break;

          default:
#ifdef SNPRINTF_STRICT
            return -1;
#else
            /* is this an error ? maybe bail out */
            PUT_CHAR('%', &data);
            is_continue = 0;
            break;
#endif
        } /* end switch */
      } /* end of while */
    } else { /* not % */
      PUT_CHAR(*data.pf, &data); /* add the char the string */
    }
  }

  if (data.ps != NULL) {
    *data.ps = '\0'; /* the end ye ! */
  }

  return (int)data.counter;
}

int SNPRINTF_PREFIX(snprintf)(char *string, size_t length, const char *format, ...) {
  int rval;
  va_list args;

  va_start(args, format);
  rval = SNPRINTF_PREFIX(vsnprintf)(string, length, format, args);
  va_end(args);

  return rval;
}


#ifdef __clang__
#pragma clang diagnostic pop
#endif
