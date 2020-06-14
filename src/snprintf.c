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

 Formating conversion flags:
   - justify left
   + Justify right or put a plus if number
   # prefix 0x, 0X for hexa and 0 for octal
   * precision/witdth is specify as an (int) in the arguments
 ' ' leave a blank for number with no sign
   l the later should be a long
   h the later should be a short

Format:
  snprintf(holder, sizeof_holder, format, ...)

Return values:
  (sizeof_holder - 1)


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

/*
 * For the FLOATING POINT FORMAT :
 *  the challenge was finding a way to
 *  manipulate the Real numbers without having
 *  to resort to mathematical function(it
 *  would require to link with -lm) and not
 *  going down to the bit pattern(not portable)
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

/*
 * size of the buffer for the integral part
 * and the fraction part
 */
#define SNP_MAX_INT 99 + 1 /* 1 for the null */
#define SNP_MAX_FRACT 29 + 1

/*
 * numtoa() uses PRIVATE buffers to store the results,
 * So this function is not reentrant
 */
#define itoa(n) numtoa(n, 10, 0, (char **)0)
#define otoa(n) numtoa(n, 8, 0, (char **)0)
#define htoa(n) numtoa(n, 16, 0, (char **)0)
#define dtoa(n, p, f) numtoa(n, 10, p, f)

/* this struct holds everything we need */
struct DATA {
  int length;
  int counter;
  char *holder;
  const char *pf;
  /* FLAGS */
  int width, precision;
  int justify;
  int square, space, dot, star_w, star_p, a_long;
  char pad;
  char rfu[3]; // slop
};

/* those are defines specific to snprintf to hopefully
 * make the code clearer :-)
 */
#define SNP_RIGHT 1
#define SNP_LEFT 0
#define SNP_NOT_FOUND -1
#define SNP_FOUND 1
#define SNP_LEN_LONG 1
#define SNP_LEN_LONG_LONG 2
#define SNP_LEN_SHORT 3
#define SNP_LEN_CHAR 4
#define SNP_MAX_FIELD 15
#define SNP_DOT 1

/* the conversion flags */
#define isflag(c) (                                   \
  (c) == '#' || (c) == ' ' ||                         \
  (c) == '*' || (c) == '+' ||                         \
  (c) == '-' || (c) == '.' ||                         \
  isdigit(c))

/* round off to the precision */
#define ROUND(d, p) \
  ((d < 0.) ? d - pow_10(-(p)->precision) * 0.5 : d + pow_10(-(p)->precision) * 0.5)

/* set default precision */
#define DEF_PREC(p)                                   \
  if ((p)->precision == SNP_NOT_FOUND) {              \
    (p)->precision = 6;                               \
  }

/* put a char */
#define PUT_CHAR(c, p)                                \
  if ((p)->counter < (p)->length) {                   \
    *(p)->holder++ = (c);                             \
    (p)->counter++;                                   \
  }

#define PUT_PLUS(d, p)                                \
  if ((d) > 0. && (p)->justify == SNP_RIGHT) {        \
    PUT_CHAR('+', p);                                 \
  }

#define PUT_SPACE(d, p)                               \
  if ((p)->space == SNP_FOUND && (d) > 0.) {          \
    PUT_CHAR(' ', p);                                 \
  }

/* pad right */
#define PAD_RIGHT(p)                                  \
  if ((p)->width > 0 && (p)->justify != SNP_LEFT) {   \
    for (; (p)->width > 0; (p)->width--) {            \
      PUT_CHAR((p)->pad, p);                          \
    }                                                 \
  }

/* pad left */
#define PAD_LEFT(p)                                   \
  if ((p)->width > 0 && (p)->justify == SNP_LEFT) {   \
    for (; (p)->width > 0; (p)->width--) {            \
      PUT_CHAR((p)->pad, p);                          \
    }                                                 \
  }

/* if width and prec. in the args */
#define STAR_ARGS(p)                                  \
  if ((p)->star_w == SNP_FOUND) {                     \
    (p)->width = va_arg(args, int);                   \
  }                                                   \
  if ((p)->star_p == SNP_FOUND) {                     \
    (p)->precision = va_arg(args, int);               \
  }

/* put number of given type to d */
#define INPUT_NUMBER(p, type, d)                      \
  if ((p)->a_long == SNP_LEN_LONG_LONG) {             \
    (d) = va_arg(args, type long long);               \
  } else if ((p)->a_long == SNP_LEN_LONG) {           \
    (d) = va_arg(args, type long);                    \
  } else {                                            \
    type int a = va_arg(args, type int);              \
    if ((p)->a_long == SNP_LEN_SHORT) {               \
      (d) = (type short)a;                            \
    } else if ((p)->a_long == SNP_LEN_CHAR) {         \
      (d) = (type char)a;                             \
    } else {                                          \
      (d) = a;                                        \
    }                                                 \
  }

/* the floating point stuff */
static double pow_10(int);
static int log_10(double);
static double integral(double, double *);
static char *numtoa(double, int, int, char **);

/* for the format */
static void conv_flag(char *, struct DATA *);
static void floating(struct DATA *, double);
static void exponent(struct DATA *, double);
static void decimal(struct DATA *, double);
static void octal(struct DATA *, double);
static void hexa(struct DATA *, double);
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

  if (r < 0.) {
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

#define PRECISION 1.e-6
/* 
 * return an ascii representation of the integral part of the number
 * and set fract to be an ascii representation of the fraction part
 * the container for the fraction and the integral part or staticly
 * declare with fix size 
 */
static char *numtoa(double number, int base, int precision, char **fract) {
  int i, j;
  double ip, fp; /* integer and fraction part */
  double fraction;
  int digits = SNP_MAX_INT - 1;
  static char integral_part[SNP_MAX_INT];
  static char fraction_part[SNP_MAX_FRACT];
  double sign;
  int ch;

  /* taking care of the obvious case: 0.0 */
  if (number == 0.) {
    integral_part[0] = '0';
    integral_part[1] = '\0';
    fraction_part[0] = '0';
    fraction_part[1] = '\0';

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
  for (i = 0, fp = fraction; precision > 0 && i < SNP_MAX_FRACT; i++, precision--) {
    fraction_part[i] = (char)(int)((fp + PRECISION) * 10. + '0');
    if (!isdigit(fraction_part[i])) { /* underflow ? */
      break;
    }
    fp = (fp * 10.0) - (double)(long)((fp + PRECISION) * 10.);
  }
  fraction_part[i] = '\0';

  if (fract != (char **)0) {
    *fract = fraction_part;
  }

  return integral_part;
}

/* for %d and friends, it puts in holder
 * the representation with the right padding
 */
static void decimal(struct DATA *p, double d) {
  char *tmp;

  tmp = itoa(d);
  p->width -= strlen(tmp);
  PAD_RIGHT(p);
  PUT_PLUS(d, p);
  PUT_SPACE(d, p);
  while (*tmp) { /* the integral */
    PUT_CHAR(*tmp, p);
    tmp++;
  }
  PAD_LEFT(p);
}

/* for %o octal representation */
static void octal(struct DATA *p, double d) {
  char *tmp;

  tmp = otoa(d);
  p->width -= strlen(tmp);
  PAD_RIGHT(p);
  if (p->square == SNP_FOUND) /* had prefix '0' for octal */
    PUT_CHAR('0', p);
  while (*tmp) { /* octal */
    PUT_CHAR(*tmp, p);
    tmp++;
  }
  PAD_LEFT(p);
}

/* for %x %X hexadecimal representation */
static void hexa(struct DATA *p, double d) {
  char *tmp;

  tmp = htoa(d);
  p->width -= strlen(tmp);
  PAD_RIGHT(p);
  if (p->square == SNP_FOUND) { /* prefix '0x' for hexa */
    PUT_CHAR('0', p);
    PUT_CHAR(*p->pf, p);
  }
  while (*tmp) { /* hexa */
    PUT_CHAR((*p->pf == 'X' ? (char)toupper((unsigned char)*tmp) : *tmp), p);
    tmp++;
  }
  PAD_LEFT(p);
}

/* %s strings */
static void strings(struct DATA *p, char *tmp) {
  int i = (int)strlen(tmp);
  if (p->precision != SNP_NOT_FOUND) { /* the smallest number */
    i = (i < p->precision ? i : p->precision);
  }
  p->width -= i;
  PAD_RIGHT(p);
  while (i-- > 0) { /* put the sting */
    PUT_CHAR(*tmp, p);
    tmp++;
  }
  PAD_LEFT(p);
}

/* %f or %g  floating point representation */
static void floating(struct DATA *p, double d) {
  char *tmp, *tmp2;
  int i;

  DEF_PREC(p);
  d = ROUND(d, p);
  tmp = dtoa(d, p->precision, &tmp2);
  /* calculate the padding. 1 for the dot */
  p->width = p->width - ((d > 0. && p->justify == SNP_RIGHT) ? 1 : 0) -
    ((p->space == SNP_FOUND) ? 1 : 0) - (int)strlen(tmp) - p->precision - 1;
  if (p->precision == 0) {
    p->width += 1;
  }
  PAD_RIGHT(p);
  PUT_PLUS(d, p);
  PUT_SPACE(d, p);
  while (*tmp) { /* the integral */
    PUT_CHAR(*tmp, p);
    tmp++;
  }
  if (p->precision != 0 || p->square == SNP_FOUND) {
    PUT_CHAR('.', p);                 /* put the '.' */
  }
  if (*p->pf == 'g' || *p->pf == 'G') { /* smash the trailing zeros */
    for (i = (int)strlen(tmp2) - 1; i >= 0 && tmp2[i] == '0'; i--) {
      tmp2[i] = '\0';
    }
  }
  for (; *tmp2; tmp2++) {
    PUT_CHAR(*tmp2, p); /* the fraction */
  }

  PAD_LEFT(p);
}

/* %e %E %g exponent representation */
static void exponent(struct DATA *p, double d) {
  char *tmp, *tmp2;
  int j, i;

  DEF_PREC(p);
  j = log_10(d);
  d = d / pow_10(j); /* get the Mantissa */
  d = ROUND(d, p);
  tmp = dtoa(d, p->precision, &tmp2);
  /* 1 for unit, 1 for the '.', 1 for 'e|E',
   * 1 for '+|-', 3 for 'exp' */
  /* calculate how much padding need */
  p->width = p->width -
    ((d > 0. && p->justify == SNP_RIGHT) ? 1 : 0) -
    ((p->space == SNP_FOUND) ? 1 : 0) - p->precision - 7;
  PAD_RIGHT(p);
  PUT_PLUS(d, p);
  PUT_SPACE(d, p);
  while (*tmp) { /* the integral */
    PUT_CHAR(*tmp, p);
    tmp++;
  }
  if (p->precision != 0 || p->square == SNP_FOUND) {
    PUT_CHAR('.', p);                 /* the '.' */
  }
  if (*p->pf == 'g' || *p->pf == 'G') { /* smash the trailing zeros */
    for (i = (int)strlen(tmp2) - 1; i >= 0 && tmp2[i] == '0'; i--) {
      tmp2[i] = '\0';
    }
  }
  for (; *tmp2; tmp2++) {
    PUT_CHAR(*tmp2, p); /* the fraction */
  }

  if (*p->pf == 'g' || *p->pf == 'e') { /* the exponent put the 'e|E' */
    PUT_CHAR('e', p);
  } else {
    PUT_CHAR('E', p);
  }
  if (j > 0) { /* the sign of the exp */
    PUT_CHAR('+', p);
  } else {
    PUT_CHAR('-', p);
    j = -j;
  }
  tmp = itoa((double)j);
  if (j < 9) { /* need to pad the exponent with 0 '000' */
    PUT_CHAR('0', p);
    PUT_CHAR('0', p);
  } else if (j < 99) {
    PUT_CHAR('0', p);
  }
  while (*tmp) { /* the exponent */
    PUT_CHAR(*tmp, p);
    tmp++;
  }
  PAD_LEFT(p);
}

/* initialize the conversion specifiers */
static void conv_flag(char *s, struct DATA *p) {
  char number[SNP_MAX_FIELD / 2];
  int i;

  p->precision = p->width = SNP_NOT_FOUND;
  p->star_w = p->star_p = SNP_NOT_FOUND;
  p->square = p->space = SNP_NOT_FOUND;
  p->a_long = p->justify = SNP_NOT_FOUND;
  p->pad = ' ';
  p->dot = SNP_NOT_FOUND;

  for (; s && *s; s++) {
    switch (*s) {
      case ' ':
        p->space = SNP_FOUND;
        break;

      case '#':
        p->square = SNP_FOUND;
        break;

      case '*':
        if (p->width == SNP_NOT_FOUND) {
          p->width = p->star_w = SNP_FOUND;
        } else {
          p->precision = p->star_p = SNP_FOUND;
        }
        break;

      case '+':
        p->justify = SNP_RIGHT;
        break;

      case '-':
        p->justify = SNP_LEFT;
        break;

      case '.':
        if (p->width == SNP_NOT_FOUND) {
          p->width = 0;
        }
        p->dot = SNP_DOT;
        break;

      case '0':
        p->pad = '0';
        if (p->dot == SNP_DOT) {
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
      case '9': /* gob all the digits */
        for (i = 0; isdigit(*s); i++, s++) {
          if (i < SNP_MAX_FIELD / 2 - 1) {
            number[i] = *s;
          }
        }
        number[i] = '\0';
        if (p->width == SNP_NOT_FOUND) {
          p->width = atoi(number);
        } else {
          p->precision = atoi(number);
        }
        s--; /* went to far go back */
        break;
    }
  }
}

int vsnprintf(char *string, size_t length, const char *format, va_list args) {
  struct DATA data;
  char conv_field[SNP_MAX_FIELD];
  double d; /* temporary holder */
  int state;
  int i;

  data.length = (int)length - 1; /* leave room for '\0' */
  data.holder = string;
  data.pf = format;
  data.counter = 0;

  /* sanity check, the string must be > 1 */
  if (length < 1) {
    return -1;
  }

  for (; *data.pf && (data.counter < data.length); data.pf++) {
    if (*data.pf == '%') { /* we got a magic % cookie */
      conv_flag((char *)0, &data); /* initialise format flags */
      for (state = 1; *data.pf && state; ) {
        switch (*(++data.pf)) {
          case '\0': /* a NULL here ? ? bail out */
            *data.holder = '\0';
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

          case 'u':
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, unsigned, d);
            decimal(&data, d);
            state = 0;
            break;

          case 'i':
          case 'd': /* decimal */
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, signed, d);
            decimal(&data, d);
            state = 0;
            break;

          case 'o': /* octal */
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, unsigned, d);
            octal(&data, d);
            state = 0;
            break;

          case 'x':
          case 'X': /* hexadecimal */
            STAR_ARGS(&data);
            INPUT_NUMBER(&data, unsigned, d);
            hexa(&data, d);
            state = 0;
            break;

          case 'c': /* character */
            d = va_arg(args, int);
            PUT_CHAR((char)d, &data);
            state = 0;
            break;

          case 's': /* string */
            STAR_ARGS(&data);
            strings(&data, va_arg(args, char *));
            state = 0;
            break;

          case 'n':
            *(va_arg(args, int *)) = data.counter; /* what's the count ? */
            state = 0;
            break;

          case 'l':
            data.a_long = (data.a_long == SNP_LEN_LONG) ? SNP_LEN_LONG_LONG : SNP_LEN_LONG;
            break;

          case 'h':
            data.a_long = (data.a_long == SNP_LEN_SHORT) ? SNP_LEN_CHAR : SNP_LEN_SHORT;
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
          case '9':
            /* initialize width and precision */
            for (i = 0; isflag(*data.pf); i++, data.pf++) {
              if (i < SNP_MAX_FIELD - 1) {
                conv_field[i] = *data.pf;
              }
            }
            conv_field[i] = '\0';
            conv_flag(conv_field, &data);
            data.pf--; /* went to far go back */
            break;

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

  *data.holder = '\0'; /* the end ye ! */

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
