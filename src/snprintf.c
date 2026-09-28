// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com

/**
 * Portable snprintf() implementation.
 * @version 2.3
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
 * By default, floating-point conversion uses self-contained helpers so it
 * works on targets without <math.h> or libm. Define SNPRINTF_USE_MATH to use
 * math.h helpers when the header and library are available.
 *
 * Points:
 *  - split the value into its integral and fractional parts;
 *  - estimate decimal magnitude without a libm dependency;
 *  - extract digits one decimal place at a time;
 *  - reverse them into normal left-to-right output order;
 *  - reuse the same approach for mantissa/exponent rendering and %g/%G trim logic.
 *
 * This is an intentionally limited renderer for the formatter's supported
 * conversions, not a general-purpose IEEE-754 conversion engine.
 */

#include <ctype.h>
#include <float.h>
#include <limits.h>
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

/** Round off to the precision. */
#define ROUND_TO_PRECISION(d, p) \
  ((d < 0.) ? d - pow_10(-(p)->precision) * 0.5 : d + pow_10(-(p)->precision) * 0.5)

/** Put a @p c character to output buffer if there is enough space. */
#define PUT_CHAR(c, p)                                  \
  if ((p)->counter < (p)->ps_size) {                    \
    if ((p)->ps != NULL) {                              \
      *(p)->ps++ = (c);                                 \
    }                                                   \
    (p)->counter++;                                     \
  }

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
#define PUT_PLUS(d, p)                                  \
  if ((d) > 0 && (p)->is_plus) {                        \
    PUT_CHAR('+', p);                                   \
  }

/** Put an optional leading space if the number is positive and the flag is set. */
#define PUT_SPACE(d, p)                                 \
  if ((p)->is_space && (d) > 0) {                       \
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
      PUT_REPEAT((p)->pad, p, (p)->width);              \
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
  } else if ((p)->precision >= MAX_FRACTION_SIZE) {     \
    (p)->precision = MAX_FRACTION_SIZE - 1;             \
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

/** Find the nth power of 10. */
static double pow_10(int n) {
#ifdef SNPRINTF_USE_MATH
  return pow(10., (double)n);
#else
  int i = 1;
  double p = 1., m;

  if (n < 0) {
    n = -n;
    m = .1;
  } else {
    m = 10.;
  }

  for (; i <= n; i++) {
    p *= m;
  }

  return p;
#endif
}

/**
 * Decimal rendering strategy for floating-point values.
 *
 * Points:
 *  - split the number into integral and fractional parts with integral();
 *  - estimate decimal magnitude using log_10() and powers of 10;
 *  - extract digits one decimal place at a time and reverse them into the
 *    normal left-to-right order;
 *  - reuse the same method for mantissa/exponent rendering with %e/%E;
 *  - trim trailing zeros for %g/%G to keep libc-like output formatting.
 *
 * This is not a fully general IEEE-754 conversion routine, but it is a compact,
 * portable approach that matches the project's formatting goals for the
 * supported floating-point conversions.
 */

/**
 * Approximate the base-10 magnitude of @p r for decimal splitting.
 *
 * The default implementation is a lightweight helper for locating digit
 * boundaries. SNPRINTF_USE_MATH delegates this calculation to log10().
 */
static int log_10(double r) {
#ifdef SNPRINTF_USE_MATH
  if (r == 0.) {
    return 0;
  }

  return (int)floor(log10(fabs(r)));
#else
  int i = 0;
  double result = 1.;

  if (r == 0.) {
    return 0;
  }
  
  if (r < 0.) {
    r = -r;
  }

  if (r < 1.) {
    for (; r < 1.; i--) {
      r *= 10.;
    }

    return i;
  } else {
    for (; result <= r; i++) {
      result *= 10.;
    }

    --i;
  }

  return i;
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
  int log;
  double real_integral = 0.;

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

  /* the real work :-) */
  for (log = log_10(real); log >= 0; log--) {
    double i = 0., p = pow_10(log);
    double s = (real - real_integral) / p;
    for (; i + 1. <= s; i++) {}
    real_integral += i * p;
  }

  *ip = real_integral;
  return (real - real_integral);
#endif
}

/** Maximum size of the buffer for the integral part. */
#define MAX_INTEGRAL_SIZE (99 + 1)
/** Maximum size of the buffer for the fraction part. */
#define MAX_FRACTION_SIZE (29 + 1)
/** Precision. */
#define PRECISION (1.e-6)

/**
 * Return an ASCII representation of the integral and fraction
 * part of the @p number.
 */
static void floattoa(double number, int precision,
    char *output_integral, size_t output_integral_size,
    char *output_fraction, size_t output_fraction_size) {

  size_t i, j;
  int is_negative = 0;
  double ip, fp; /* integer and fraction part */
  double fraction;

  /* taking care of the obvious case: 0.0 */
  if (number == 0.) {
    output_integral[0] = output_fraction[0] = '0';
    output_integral[1] = output_fraction[1] = '\0';

    return;
  }

  /* for negative numbers */
  if (number < 0.) {
    number = -number;
    is_negative = 1;
    output_integral_size--; /* sign consume one digit */
  }

  fraction = integral(number, &ip);
  number = ip;
  /* do the integral part */
  if (ip == 0.) {
    output_integral[0] = '0';
    i = 1;
  } else {
    for (i = 0; i < output_integral_size - 1 && number != 0.; ++i) {
      number /= 10;
      /* force to round */
      output_integral[i] = (char)((integral(number, &ip) + PRECISION) * 10) + '0';
      if (!isdigit(output_integral[i])) { /* bail out overflow !! */
        break;
      }
      number = ip;
    }
  }

  /* Oh No !! out of bound, ho well fill it up ! */
  if (number != 0.) {
    for (i = 0; i < output_integral_size - 1; ++i) {
      output_integral[i] = '9';
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

  /* the fractional part */
  for (i = 0, fp = fraction; precision > 0 && i < output_fraction_size - 1; i++, precision--) {
    double scaled = fp * 10.;
    int digit = (int)scaled;
    output_fraction[i] = (char)digit + '0';
    if (!isdigit(output_fraction[i])) { /* underflow ? */
      break;
    }

    fp = scaled - digit;
  }
  output_fraction[i] = '\0';
}

/** Emit a sign prefix before zero-filled numeric output. */
static void emit_sign_prefix(struct DATA *p, long long value) {
  if (value < 0) {
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
  int sign = 0;

  inttoa(ll, *p->pf == 'i' || *p->pf == 'd', p->precision, 10,
    number, sizeof(number));

  if (ll < 0) {
    digits = number + 1;
    sign = 1;
  } else if (p->is_plus || p->is_space) {
    sign = 1;
  }

  p->width -= (int)strlen(digits) + sign;
  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    emit_sign_prefix(p, ll);
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    emit_sign_prefix(p, ll);
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

  if (p->is_square && *number != '\0') {
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
  int len = (int)strlen(src);

  if (p->precision != PRECISION_UNSET && len > p->precision) {
    len = p->precision;
  }

  p->width -= len;

  PAD_RIGHT(p);

  for (; len-- > 0; src++) {
    PUT_CHAR(*src, p);
  }

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
  char integral[MAX_INTEGRAL_SIZE], *pintegral = integral;
  char fraction[MAX_FRACTION_SIZE], *pfraction = fraction;
  int is_general = *p->pf == 'g' || *p->pf == 'G';
  int has_dot;

  d = ROUND_TO_PRECISION(d, p);
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
  if (d > 0. && (p->is_plus || p->is_space)) {
    p->width -= 1;
  }
  
  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    if (*pintegral == '-') {
      PUT_CHAR(*pintegral++, p);
    } else {
      PUT_PLUS(d, p);
      PUT_SPACE(d, p);
    }
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    PUT_PLUS(d, p);
    PUT_SPACE(d, p);
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
  int log = log_10(d);
  int is_general = *p->pf == 'g' || *p->pf == 'G';
  int has_dot;
  int exponent_digits;
  d /= pow_10(log); /* get the Mantissa */
  d = ROUND_TO_PRECISION(d, p);
  if (d >= 10. || d <= -10.) {
    d /= 10.;
    log++;
  }

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
  exponent_digits = log <= -100 || log >= 100 ? 3 : 2;
  p->width -= (int)strlen(integral) + (int)strlen(fraction) + has_dot +
      exponent_digits + 2;
  if (d > 0. && (p->is_plus || p->is_space)) {
    p->width -= 1;
  }

  if (p->pad == '0' && p->align != ALIGN_LEFT) {
    if (*pintegral == '-') {
      PUT_CHAR(*pintegral++, p);
    } else {
      PUT_PLUS(d, p);
      PUT_SPACE(d, p);
    }
    PUT_REPEAT('0', p, p->width);
    p->width = 0;
  } else {
    PAD_RIGHT(p);
    PUT_PLUS(d, p);
    PUT_SPACE(d, p);
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
            log = log_10(d);
            if (d != 0. && (log == -5 || log == data.precision - 1)) {
              int rounding_precision = data.precision - log - 1;
              double half = pow_10(-rounding_precision) * 0.5;
              log = log_10(d < 0. ? d - half : d + half);
            }
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
            int i = va_arg(args, int);
            PUT_CHAR((char)i, &data);
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
            void *v = va_arg(args, void *);
            data.is_square = 1;
            if (v == NULL) {
              strings(&data, "(nil)");
            } else {
              hex(&data, (long long)v);
            }
            is_continue = 0;
            break;
          }

          case 'n': /* what's the count ? */
            *(va_arg(args, int *)) = (int)data.counter;
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
