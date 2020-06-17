// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com

/*
 
Unix snprintf implementation.
Version 2.0
   
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Revision History:
2.1: by Miroslaw Toton (mirtoto)
   * fix problem with very big and very low "long long" values
   * change exponent width from 3 to 2
   * fix zero value for floating
   * support for "p" (%p)

2.0: by Miroslaw Toton (mirtoto)
   * move all defines & macros from header to codefile
   * support for "long long" (%llu, %lld, %llo, %llx)
   * fix for interpreting precision in some situations
   * fix unsigned (%u) for negative input
   * fix h & hh length of input number specifier
   * fix Clang linter warnings

1.1:
   * added changes from Miles Bader
   * corrected a bug with %f
   * added support for %#g
   * added more comments :-)

1.0:
   * supporting must ANSI syntaxic_sugars(see below)

0.0:
   * suppot %s %c %d

It understands:
   * Integer:
     %hu %llu %lu %u     unsigned
     %hd %lld %ld %d     decimal
     %ho %llo %lo %o     octal
     %hx %llx %lx %x %X  hexa
   * Floating points:
     %g %G %e %E %f      double
   * Strings:
     %s                  string
     %c                  character
     %%                  %
   * Other:
     %p                  pointer

 Formating conversion flags:
    - align left
    + Justify right or put a plus if number
    # prefix 0x, 0X for hexa and 0 for octal
    * precision/witdth is specify as an (int) in the arguments
  ' ' leave a blank for number with no sign
    l the later should be a long
   ll the later should be a long long
    h the later should be a short
   hh the later should be a char

Format:
  snprintf(string, sizeof_string, format, ...)

Return values:
  (sizeof_string - 1)


 THANKS(for the patches and ideas):
     Miles Bader
     Cyrille Rustom
     Jacek Slabocewiz
     Mike Parker(mouse)

Alain Magloire: alainm@rcsm.ee.mcgill.ca

*/

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "snprintf.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif


/*
 * For the FLOATING POINT FORMAT :
 *  the challenge was finding a way to
 *  manipulate the Real numbers without having
 *  to resort to mathematical function (it
 *  would require to link with -lm) and not
 *  going down to the bit pattern (not portable)
 *
 *  so a number, a real is:

      real = integral + fraction

      integral = ... + a(2)*10^2 + a(1)*10^1 + a(0)*10^0
      fraction = b(1)*10^-1 + b(2)*10^-2 + ...

      where:
       0 <= a(i) => 9
       0 <= b(i) => 9

    from then it was simple math
 */

/* this struct holds everything we need */
struct DATA {
  int length;
  int counter;
  char *string;
  const char *pf;

#define WIDTH_NOT_FOUND       -1

  int width;

#define PRECISION_NOT_FOUND   -1

  int precision;

#define ALIGN_DEFAULT         0
#define ALIGN_RIGHT           1
#define ALIGN_LEFT            2

  unsigned int align:2;
  unsigned int is_square:1;
  unsigned int is_space:1;
  unsigned int is_dot:1;
  unsigned int is_star_w:1;
  unsigned int is_star_p:1;

#define INT_LEN_DEFAULT       0
#define INT_LEN_LONG          1
#define INT_LEN_LONG_LONG     2
#define INT_LEN_SHORT         3
#define INT_LEN_CHAR          4

  unsigned int a_long:3;

  unsigned int rfu:6;

  char pad;

  char slop[5];
};

/* the conversion flags */
#define IS_CONV_FLAG(c) (isdigit(c) ||                  \
  (c) == '#' || (c) == ' ' ||                           \
  (c) == '*' || (c) == '+' ||                           \
  (c) == '-' || (c) == '.')

/* round off to the precision */
#define ROUND(d, p) \
  ((d < 0.) ? d - pow_10(-(p)->precision) * 0.5 : d + pow_10(-(p)->precision) * 0.5)

/* set default precision */
#define DEF_PREC(p)                                     \
  if ((p)->precision == PRECISION_NOT_FOUND) {          \
    (p)->precision = 6;                                 \
  }

/* put a char */
#define PUT_CHAR(c, p)                                  \
  if ((p)->counter < (p)->length) {                     \
    *(p)->string++ = (c);                               \
    (p)->counter++;                                     \
  }

#define PUT_PLUS(d, p)                                  \
  if ((d) > 0 && (p)->align == ALIGN_RIGHT) {           \
    PUT_CHAR('+', p);                                   \
  }

#define PUT_SPACE(d, p)                                 \
  if ((p)->is_space && (d) > 0) {                       \
    PUT_CHAR(' ', p);                                   \
  }

/* pad right */
#define PAD_RIGHT(p)                                    \
  if ((p)->width > 0 && (p)->align != ALIGN_LEFT) {     \
    for (; (p)->width > 0; (p)->width--) {              \
      PUT_CHAR((p)->pad, p);                            \
    }                                                   \
  }

/* pad left */
#define PAD_LEFT(p)                                     \
  if ((p)->width > 0 && (p)->align == ALIGN_LEFT) {     \
    for (; (p)->width > 0; (p)->width--) {              \
      PUT_CHAR((p)->pad, p);                            \
    }                                                   \
  }

/* if width and prec. in the args */
#define STAR_ARGS(p)                                    \
  if ((p)->is_star_w) {                                 \
    (p)->width = va_arg(args, int);                     \
  }                                                     \
  if ((p)->is_star_p) {                                 \
    (p)->precision = va_arg(args, int);                 \
  }

/* put number of given type to ll */
#define INPUT_NUMBER(p, type, ll)                       \
  if ((p)->a_long == INT_LEN_LONG_LONG) {               \
    (ll) = (long long)va_arg(args, type long long);     \
  } else if ((p)->a_long == INT_LEN_LONG) {             \
    (ll) = (long long)va_arg(args, type long);          \
  } else {                                              \
    type int a = va_arg(args, type int);                \
    if ((p)->a_long == INT_LEN_SHORT) {                 \
      (ll) = (type short)a;                             \
    } else if ((p)->a_long == INT_LEN_CHAR) {           \
      (ll) = (type char)a;                              \
    } else {                                            \
      (ll) = a;                                         \
    }                                                   \
  }

/* the floating point stuff */
static double pow_10(int);
static int log_10(double);
static double integral(double, double *);
static char *floattoa(double, int, int, char **);
static char *inttoa(long long, int, int);

/* for the format */
static void conv_flag(char *, struct DATA *);
static void floating(struct DATA *, double);
static void exponent(struct DATA *, double);
static void decimal(struct DATA *, long long);
static void octal(struct DATA *, long long);
static void hexa(struct DATA *, long long);
static void strings(struct DATA *, char *);

/*
 * Find the nth power of 10
 */
static double pow_10(int n) {
  int i;
  double P;

  if (n < 0) {
    for (i = 1, P = 1., n = -n; i <= n; i++) {
      P *= .1;
    }
  } else {
    for (i = 1, P = 1.; i <= n; i++) {
      P *= 10.0;
    }
  }

  return P;
}

/*
 * Find the integral part of the log in base 10 
 * Note: this not a real log10()
         I just need and approximation(integerpart) of x in:
          10^x ~= r
 * log_10(200) = 2;
 * log_10(250) = 2;
 */
static int log_10(double r) {
  int i = 0;
  double result = 1.;

  if (r == 0.) {
    return 0;
  } else if (r < 0.) {
    r = -r;
  }

  if (r < 1.) {
    while (result >= r) {
      result *= .1;
      i++;
    }

    return (-i);
  } else {
    while (result <= r) {
      result *= 10.;
      i++;
    }

    return (i - 1);
  }
}

/*
 * This function return the fraction part of a double
 * and set in ip the integral part.
 * In many ways it resemble the modf() found on most Un*x
 */
static double integral(double real, double *ip) {
  int j;
  double i, s, p;
  double real_integral = 0.;

  /* take care of the obvious */
  /* equal to zero ? */
  if (real == 0.) {
    *ip = 0.;
    return (0.);
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
  for (j = log_10(real); j >= 0; j--) {
    p = pow_10(j);
    s = (real - real_integral) / p;
    i = 0.;

    while (i + 1. <= s) {
      i++;
    }
    real_integral += i * p;
  }
  *ip = real_integral;

  return (real - real_integral);
}

/* size of the buffer for the integral part */
#define MAX_INTEGRAL_SIZE (99 + 1)
/* size of the buffer for the fraction part */
#define MAX_FRACTION_SIZE (29 + 1)
/* precision */
#define PRECISION (1.e-6)

/* 
 * return an ascii representation of the integral part of the number
 * and set fract to be an ascii representation of the fraction part
 * the container for the fraction and the integral part or staticly
 * declare with fix size 
 */
static char *floattoa(double number, int base, int precision, char **fract) {
  int i, j, ch;
  double ip, fp; /* integer and fraction part */
  double fraction;
  int digits = MAX_INTEGRAL_SIZE - 1;
  static char integral_part[MAX_INTEGRAL_SIZE];
  static char fraction_part[MAX_FRACTION_SIZE];
  double sign;

  /* taking care of the obvious case: 0.0 */
  if (number == 0.) {
    memcpy(integral_part, "0", 2);
    memcpy(fraction_part, "0", 2);

    if (fract != NULL) {
      *fract = fraction_part;
    }
    
    return integral_part;
  }

  /* for negative numbers */
  if ((sign = number) < 0.) {
    number = -number;
    digits--; /* sign consume one digit */
  }

  fraction = integral(number, &ip);
  number = ip;
  /* do the integral part */
  if (ip == 0.) {
    integral_part[0] = '0';
    i = 1;
  } else {
    for (i = 0; i < digits && number != 0.; ++i) {
      number /= base;
      fp = integral(number, &ip);
      ch = (int)((fp + PRECISION) * base); /* force to round */
      integral_part[i] = (char)((ch <= 9) ? ch + '0' : ch + 'a' - 10);
      if (!isxdigit(integral_part[i])) { /* bail out overflow !! */
        break;
      }
      number = ip;
    }
  }

  /* Oh No !! out of bound, ho well fill it up ! */
  if (number != 0.) {
    for (i = 0; i < digits; ++i) {
      integral_part[i] = '9';
    }
  }

  /* put the sign ? */
  if (sign < 0.) {
    integral_part[i++] = '-';
  }

  integral_part[i] = '\0';

  /* reverse every thing */
  for (i--, j = 0; j < i; j++, i--) {
    char tmp = integral_part[i];
    integral_part[i] = integral_part[j];
    integral_part[j] = tmp;
  }

  /* the fractionnal part */
  for (i = 0, fp = fraction; precision > 0 && i < MAX_FRACTION_SIZE; i++, precision--) {
    fraction_part[i] = (char)(int)((fp + PRECISION) * 10. + '0');
    if (!isdigit(fraction_part[i])) { /* underflow ? */
      break;
    }
    fp = (fp * 10.0) - (double)(long)((fp + PRECISION) * 10.);
  }
  fraction_part[i] = '\0';

  if (fract != NULL) {
    *fract = fraction_part;
  }

  return integral_part;
}

// is_signed == 0 - interpret 'number' as unsigned
// is_signed == 1 - interpret 'number' as signed
static char *inttoa(long long number, int is_signed, int base) {    
  static char integral_part[MAX_INTEGRAL_SIZE];
  int i = 0, j; 

  if (number != 0) {
    unsigned long long n;

    if (is_signed && number < 0) {
      n = (unsigned long long)-number;
    } else {
      n = (unsigned long long)number;
    }

    while (n != 0 && i < MAX_INTEGRAL_SIZE - 1) {
      int r = (int)(n % (unsigned long long)(base));
      integral_part[i++] = (char)r + (r < 10 ? '0' : 'a' - 10);
      n /= (unsigned long long)(base);
    }

    /* put the sign ? */
    if (is_signed && number < 0) {
      integral_part[i++] = '-';
    }

    integral_part[i] = '\0';
    
    /* reverse every thing */
    for (i--, j = 0; j < i; j++, i--) {
      char tmp = integral_part[i];
      integral_part[i] = integral_part[j];
      integral_part[j] = tmp;
    }
  } else {
    memcpy(integral_part, "0", 2);
  }

  return integral_part;
}

/* for %d and friends, it puts in string
 * the representation with the right padding
 */
static void decimal(struct DATA *p, long long d) {
  char *number = inttoa(d, *p->pf == 'i' || *p->pf == 'd', 10);
  p->width -= strlen(number);
  PAD_RIGHT(p);
  PUT_PLUS(d, p);
  PUT_SPACE(d, p);
  while (*number != '\0') {
    PUT_CHAR(*number, p);
    number++;
  }
  PAD_LEFT(p);
}

/* for %o octal representation */
static void octal(struct DATA *p, long long d) {
  char *number = inttoa(d, 0, 8);
  p->width -= strlen(number);
  PAD_RIGHT(p);
  if (p->is_square) { /* had prefix '0' for octal */
    PUT_CHAR('0', p);
  }
  while (*number != '\0') {
    PUT_CHAR(*number, p);
    number++;
  }
  PAD_LEFT(p);
}

/* for %x %X hexadecimal representation */
static void hexa(struct DATA *p, long long d) {
  char *number = inttoa(d, 0, 16);
  p->width -= strlen(number);
  PAD_RIGHT(p);
  if (p->is_square) { /* prefix '0x' for hexa */
    PUT_CHAR('0', p);
    PUT_CHAR(*p->pf == 'p' ? 'x' : *p->pf, p);
  }
  while (*number != '\0') {
    PUT_CHAR((*p->pf == 'X' ? (char)toupper(*number) : *number), p);
    number++;
  }
  PAD_LEFT(p);
}

/* %s strings */
static void strings(struct DATA *p, char *str) {
  int len = (int)strlen(str);
  if (p->precision != PRECISION_NOT_FOUND && len > p->precision) { /* the smallest number */
    len = p->precision;
  }
  p->width -= len;
  PAD_RIGHT(p);
  while (len-- > 0) { /* put the sting */
    PUT_CHAR(*str, p);
    str++;
  }
  PAD_LEFT(p);
}

/* %f or %g  floating point representation */
static void floating(struct DATA *p, double d) {
  char *integral, *fraction;

  DEF_PREC(p);
  d = ROUND(d, p);
  integral = floattoa(d, 10, p->precision, &fraction);
  /* calculate the padding. 1 for the dot */
  if (d > 0. && p->align == ALIGN_RIGHT) {
    p->width -= 1;  
  }
  p->width -= p->is_space + (int)strlen(integral) + p->precision + 1;
  if (p->precision == 0) {
    p->width += 1;
  }
  PAD_RIGHT(p);
  PUT_PLUS(d, p);
  PUT_SPACE(d, p);
  while (*integral != '\0') { /* the integral */
    PUT_CHAR(*integral, p);
    integral++;
  }
  if (p->precision != 0 || p->is_square) {
    PUT_CHAR('.', p);                 /* put the '.' */
  }
  if (*p->pf == 'g' || *p->pf == 'G') { /* smash the trailing zeros */
    int i;
    for (i = (int)strlen(fraction) - 1; i >= 0 && fraction[i] == '0'; i--) {
      fraction[i] = '\0';
    }
  }
  for (; *fraction != '\0'; fraction++) {
    PUT_CHAR(*fraction, p); /* the fraction */
  }

  PAD_LEFT(p);
}

/* %e %E %g exponent representation */
static void exponent(struct DATA *p, double d) {
  char *integral, *fraction, *exponent;
  int j, i;

  DEF_PREC(p);
  j = log_10(d);
  d = d / pow_10(j); /* get the Mantissa */
  d = ROUND(d, p);
  integral = floattoa(d, 10, p->precision, &fraction);
  /* 1 for unit, 1 for the '.', 1 for 'e|E',
   * 1 for '+|-', 3 for 'exp' */
  /* calculate how much padding need */
  if (d > 0. && p->align == ALIGN_RIGHT) {
    p->width -= 1;  
  }
  p->width -= p->is_space + p->precision + 7;
  PAD_RIGHT(p);
  PUT_PLUS(d, p);
  PUT_SPACE(d, p);
  while (*integral != '\0') { /* the integral */
    PUT_CHAR(*integral, p);
    integral++;
  }
  if (p->precision != 0 || p->is_square) {
    PUT_CHAR('.', p);                 /* the '.' */
  }
  if (*p->pf == 'g' || *p->pf == 'G') { /* smash the trailing zeros */
    for (i = (int)strlen(fraction) - 1; i >= 0 && fraction[i] == '0'; i--) {
      fraction[i] = '\0';
    }
  }
  for (; *fraction != '\0'; fraction++) {
    PUT_CHAR(*fraction, p); /* the fraction */
  }

  if (*p->pf == 'g' || *p->pf == 'e') { /* the exponent put the 'e|E' */
    PUT_CHAR('e', p);
  } else {
    PUT_CHAR('E', p);
  }
  if (j >= 0) { /* the sign of the exp */
    PUT_CHAR('+', p);
  } else {
    PUT_CHAR('-', p);
    j = -j;
  }
  exponent = inttoa(j, 0, 10);
  if (j <= 9) { /* need to pad the exponent with 0 */
    PUT_CHAR('0', p);
  }
  while (*exponent != '\0') { /* the exponent */
    PUT_CHAR(*exponent, p);
    exponent++;
  }
  PAD_LEFT(p);
}

/* initialize the conversion specifiers */
static void conv_flag(char *s, struct DATA *p) {
  p->width = WIDTH_NOT_FOUND;
  p->precision = PRECISION_NOT_FOUND;
  p->is_star_w = p->is_star_p = 0;
  p->is_square = p->is_space = 0;
  p->a_long = INT_LEN_DEFAULT;
  p->align = ALIGN_DEFAULT;
  p->pad = ' ';
  p->is_dot = 0;

  for (; s && *s != '\0'; s++) {
    switch (*s) {
      case ' ':
        p->is_space = 1;
        break;

      case '#':
        p->is_square = 1;
        break;

      case '*':
        if (p->width == WIDTH_NOT_FOUND) {
          p->width = 1;
          p->is_star_w = 1;
        } else {
          p->precision = 1;
          p->is_star_p = 1;
        }
        break;

      case '+':
        p->align = ALIGN_RIGHT;
        break;

      case '-':
        p->align = ALIGN_LEFT;
        break;

      case '.':
        if (p->width == WIDTH_NOT_FOUND) {
          p->width = 0;
        }
        p->is_dot = 1;
        break;

      case '0':
        p->pad = '0';
        if (p->is_dot) {
          p->precision = 0;
          break;
        }
        p->pad = '0';
        break;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': { /* gob all the digits */
        char number[0x10] = {0, };
        int i;

        for (i = 0; i < (int)sizeof(number) - 1 && isdigit(*s); i++, s++) {
          number[i] = *s;
        }

        if (p->width == WIDTH_NOT_FOUND) {
          p->width = atoi(number);
        } else {
          p->precision = atoi(number);
        }

        s--; /* went to far go back */

        break;
      }

      default:
        break;
    }
  }
}

int vsnprintf(char *string, size_t length, const char *format, va_list args) {
  struct DATA data;
  double d; /* temporary holder */
  long long ll; /* temporary holder */
  int state;
  int i;

  data.length = (int)length - 1; /* leave room for '\0' */
  data.string = string;
  data.pf = format;
  data.counter = 0;

  /* sanity check, the string must be > 1 */
  if (length < 1) {
    return -1;
  }

  for (; *data.pf && (data.counter < data.length); data.pf++) {
    if (*data.pf == '%') { /* we got a magic % cookie */
      conv_flag(NULL, &data); /* initialise format flags */
      for (state = 1; *data.pf && state; ) {
        switch (*(++data.pf)) {
          case '\0': /* a NULL here ? ? bail out */
            *data.string = '\0';
            return data.counter;

          case 'f': /* float, double */
          case 'F':
            STAR_ARGS(&data);
            d = va_arg(args, double);
            floating(&data, d);
            state = 0;
            break;

          case 'g':
          case 'G':
            STAR_ARGS(&data);
            DEF_PREC(&data);
            d = va_arg(args, double);
            i = log_10(d);
            /*
             * for '%g|%G' ANSI: use f if exponent
             * is in the range or [-4,p] exclusively
             * else use %e|%E
             */
            if (-4 < i && i < data.precision) {
              floating(&data, d);
            } else {
              exponent(&data, d);
            }
            state = 0;
            break;

          case 'e':
          case 'E': /* Exponent double */
            STAR_ARGS(&data);
            d = va_arg(args, double);
            exponent(&data, d);
            state = 0;
            break;

          case 'u': /* unsigned decimal */
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, unsigned, ll);
            decimal(&data, ll);
            state = 0;
            break;

          case 'i':
          case 'd': /* decimal */
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, signed, ll);
            decimal(&data, ll);
            state = 0;
            break;

          case 'o': /* octal */
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, unsigned, ll);
            octal(&data, ll);
            state = 0;
            break;

          case 'x':
          case 'X': /* hexadecimal */
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, unsigned, ll);
            hexa(&data, ll);
            state = 0;
            break;

          case 'c': /* character */
            ll = va_arg(args, int);
            PUT_CHAR((char)ll, &data);
            state = 0;
            break;

          case 's': /* string */
            STAR_ARGS(&data);
            strings(&data, va_arg(args, char *));
            state = 0;
            break;

          case 'p': /* pointer */
            data.is_square = 1;
            ll = (long long)va_arg(args, void *);
            ll == 0 ? strings(&data, "(nil)") : hexa(&data, ll);
            state = 0;
            break;

          case 'n':
            *(va_arg(args, int *)) = data.counter; /* what's the count ? */
            state = 0;
            break;

          case 'l':
            if (data.a_long == INT_LEN_LONG) {
              data.a_long = INT_LEN_LONG_LONG;
            } else {
              data.a_long = INT_LEN_LONG;
            }
            break;

          case 'h':
            if (data.a_long == INT_LEN_SHORT) {
              data.a_long = INT_LEN_CHAR;
            } else {
              data.a_long = INT_LEN_SHORT;
            }
            break;

          case '%': /* nothing just % */
            PUT_CHAR('%', &data);
            state = 0;
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
          case '9': {
            char conv_flags[0x10] = {0, };
            /* initialize width and precision */
            for (i = 0; i < (int)sizeof(conv_flags) - 1 && IS_CONV_FLAG(*data.pf); i++, data.pf++) {
              conv_flags[i] = *data.pf;
            }
            conv_flag(conv_flags, &data);
            data.pf--; /* went to far go back */
            break;
          }

          default:
            /* is this an error ? maybe bail out */
            state = 0;
            break;
        } /* end switch */
      } /* end of for state */
    } else { /* not % */
      PUT_CHAR(*data.pf, &data); /* add the char the string */
    }
  }

  *data.string = '\0'; /* the end ye ! */

  return data.counter;
}

int snprintf(char *string, size_t length, const char *format, ...) {
  int rval;
  va_list args;

  va_start(args, format);
  rval = vsnprintf(string, length, format, args);
  va_end(args);

  return rval;
}


#ifdef __clang__
#pragma clang diagnostic pop
#endif
