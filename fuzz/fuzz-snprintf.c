/*
 * Differential fuzz test of snprintf().
 *
 * Every random format is run through the C library and through this
 * implementation and the results are compared: the output, the returned
 * length, the length calculated for a NULL buffer, and the output truncated
 * to a random buffer size (which must be a prefix of the full output, must be
 * terminated, and must not touch a single byte after the buffer).
 *
 * The sequence of formats only depends on the seed, so a failure is easy to
 * reproduce:
 *
 *   fuzz-snprintf [seed [iterations]]
 *
 * Build it with -DUSE_SNPRINTF_PREFIX, so both snprintf() can be called, see
 * the Makefile in this directory.
 *
 * What is left out, because it is a documented difference to the C library
 * (see snprintf.h) or because the C standard does not define it:
 *  - the # flag of %g and %G (glibc drops digits when rounding carries to the
 *    next power of ten, the C standard and this implementation do not),
 *  - a precision of a floating-point conversion above 29, and of an integer
 *    conversion above 60,
 *  - a NULL string with a precision, the sign of NaN, the character '\0',
 *  - long double, wide characters, %a and %n.
 * The flags the C standard does not define for a conversion are used too, as
 * long as the C library gives them a meaning that is the same everywhere.
 */
#ifndef USE_SNPRINTF_PREFIX
#error "build with -DUSE_SNPRINTF_PREFIX, so both snprintf() can be called"
#endif

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The C library, before snprintf.h renames the functions of this project. */
static int libc_vsnprintf(char *s, size_t n, const char *f, va_list ap) {
  return vsnprintf(s, n, f, ap);
}

#include "snprintf.h"


/* The random numbers do not depend on the C library, to repeat a run anywhere. */
static uint64_t rng_state;

static uint64_t rnd64(void) { /* splitmix64 */
  uint64_t z = (rng_state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

/** Random number from 0 to n - 1. */
static unsigned rnd(unsigned n) {
  return (unsigned)(rnd64() % n);
}

/** Random number from 0 up to 1, excluding 1. */
static double rnd01(void) {
  return (double)(rnd64() >> 11) * (1.0 / 9007199254740992.0);
}


#define BUFFER_SIZE 1200

static unsigned long checked;
static unsigned long failed;
static char value_text[160]; /* the argument of the format, for the report */

/** Run one format through both implementations, return 1 on a mismatch. */
static int check(const char *fmt, ...) {
  char want[BUFFER_SIZE], got[BUFFER_SIZE], cut[BUFFER_SIZE + 16];
  va_list a, b, c, d;
  int want_len, got_len, null_len, cut_len, expected_cut_len;
  size_t cap, keep, i;
  const char *problem = NULL;

  va_start(a, fmt);
  va_copy(b, a);
  va_copy(c, a);
  va_copy(d, a);

  want_len = libc_vsnprintf(want, sizeof want, fmt, a);
  got_len = my_vsnprintf(got, sizeof got, fmt, b);
  null_len = my_vsnprintf(NULL, 0, fmt, c);
  if (want_len < 0 || want_len >= BUFFER_SIZE) {
    printf("the fuzz test is broken, the C library gave %d for \"%s\"\n", want_len, fmt);
    exit(2);
  }
  cap = 1 + rnd((unsigned)want_len + 2); /* from 1 to want_len + 2 */
  memset(cut, 0x5a, sizeof cut);
  cut_len = my_vsnprintf(cut, cap, fmt, d);

  va_end(a);
  va_end(b);
  va_end(c);
  va_end(d);

  keep = (size_t)want_len < cap - 1 ? (size_t)want_len : cap - 1;
#ifdef SNPRINTF_LEGACY_LENGTH
  /* the legacy mode returns the number of characters written, not the length
     of the whole output */
  expected_cut_len = (int)keep;
#else
  expected_cut_len = want_len;
#endif
  if (got_len != want_len || strcmp(got, want) != 0) {
    problem = "output";
  } else if (null_len != want_len) {
    problem = "length for a NULL buffer";
  } else if (cut_len != expected_cut_len || memcmp(cut, want, keep) != 0 || cut[keep] != '\0') {
    problem = "truncated output";
  } else {
    for (i = cap; i < cap + 8; i++) {
      if (cut[i] != 0x5a) {
        problem = "write after the end of the buffer";
      }
    }
  }

  checked++;
  if (problem == NULL) {
    return 0;
  }
  failed++;
  if (failed <= 10) {
    printf("MISMATCH, %s\n  format \"%s\", value %s\n  libc: [%.150s] (%d)\n  ours: [%.150s] (%d)\n",
        problem, fmt, value_text, want, want_len, got, got_len);
    if (problem[0] == 't' || problem[0] == 'w') {
      printf("  buffer size %lu: [%.150s] (%d)\n", (unsigned long)cap, cut, cut_len);
    }
  }
  return 1;
}


/** One conversion specification, with the arguments of its stars. */
struct spec {
  char text[64];
  int wstar, pstar, width, precision;
};

/** Run the format around @p s with one value, whatever the stars need. */
#define CALL(fmt, s, value) \
  ((s).wstar && (s).pstar ? check(fmt, (s).width, (s).precision, value) : \
   (s).wstar ? check(fmt, (s).width, value) : \
   (s).pstar ? check(fmt, (s).precision, value) : check(fmt, value))

/** Make a random specification of a conversion, flags from @p flags. */
static void make_spec(struct spec *s, const char *flags, int max_precision,
    const char *length, char conversion) {
  char *t = s->text;
  unsigned i, n = flags[0] != '\0' ? rnd(4) : 0;

  *t++ = '%';
  for (i = 0; i < n; i++) {
    *t++ = flags[rnd((unsigned)strlen(flags))];
  }
  s->wstar = s->pstar = 0;
  s->width = s->precision = 0;
  switch (rnd(4)) {
    case 1:
      t += sprintf(t, "%u", 1 + rnd(40));
      break;
    case 2: /* a negative width means the - flag */
      *t++ = '*';
      s->wstar = 1;
      s->width = (int)rnd(46) - 5;
      break;
    default:
      break;
  }
  if (max_precision >= 0) {
    switch (rnd(5)) {
      case 1: /* a lone dot means precision 0 */
        *t++ = '.';
        break;
      case 2:
        t += sprintf(t, ".%u", rnd((unsigned)max_precision + 1));
        break;
      case 3: /* a negative precision means none */
        *t++ = '.';
        *t++ = '*';
        s->pstar = 1;
        s->precision = (int)rnd((unsigned)max_precision + 4) - 3;
        break;
      default:
        break;
    }
  }
  t += sprintf(t, "%s%c", length, conversion);
}

/** Put some text around the specification. */
static void make_format(char *fmt, const struct spec *s) {
  static const char *before[] = {"", "", "x=", "[", "%% ", "a b "};
  static const char *after[] = {"", "", "]", " tail", "%%"};
  sprintf(fmt, "%s%s%s", before[rnd(6)], s->text, after[rnd(5)]);
}


static long long int_value(void) {
  static const long long special[] = {0, 1, -1, 7, -7, 42, 255, -255, 256, 1000,
      32767, -32768, 65535, 65536, 2147483647LL, -2147483647LL - 1,
      4294967295LL, 4294967296LL, 9223372036854775807LL,
      -9223372036854775807LL - 1, 123456789};
  long long v;

  if (rnd(3) == 0) {
    return special[rnd(sizeof special / sizeof *special)];
  }
  v = (long long)(rnd64() >> rnd(64));
  return rnd(2) && v != LLONG_MIN ? -v : v;
}

static int fuzz_integer(void) {
  static const char *lengths[] = {"", "hh", "h", "l", "ll", "z", "t", "j"};
  static const char conversions[] = "diuoxX";
  const char *length = lengths[rnd(8)];
  char conversion = conversions[rnd(6)];
  int is_unsigned = conversion != 'd' && conversion != 'i';
  long long v = int_value();
  struct spec s;
  char fmt[128];

  make_spec(&s, "-+ #0", 60, length, conversion);
  make_format(fmt, &s);
  sprintf(value_text, "%lld", v);

#define LENGTH(name, signed_type, unsigned_type) \
  if (strcmp(length, name) == 0) { \
    return is_unsigned ? CALL(fmt, s, (unsigned_type)v) : CALL(fmt, s, (signed_type)v); \
  }
  LENGTH("", int, unsigned)
  LENGTH("hh", signed char, unsigned char)
  LENGTH("h", short, unsigned short)
  LENGTH("l", long, unsigned long)
  LENGTH("ll", long long, unsigned long long)
  LENGTH("z", ptrdiff_t, size_t)
  LENGTH("t", ptrdiff_t, size_t)
  LENGTH("j", intmax_t, uintmax_t)
#undef LENGTH
  return 0;
}

static int fuzz_string(void) {
  static const char *pool[] = {"", "a", "Hello", "Hello, World! 0123456789", "%d %s", "x y z"};
  struct spec s;
  char fmt[128];
  const char *v;

  /* the flags other than - are meaningless for a string, strict mode rejects them */
#ifdef SNPRINTF_STRICT
  make_spec(&s, "-", 30, "", 's');
#else
  make_spec(&s, "-+ #0", 30, "", 's');
#endif
  make_format(fmt, &s);
  v = pool[rnd(6)];
  if (strchr(s.text, '.') == NULL && rnd(8) == 0) {
    v = NULL;
  }
  sprintf(value_text, "%s", v != NULL ? v : "NULL");
  return CALL(fmt, s, v);
}

static int fuzz_char(void) {
  struct spec s;
  char fmt[128];
  int v = 'A' + (int)rnd(26);

  make_spec(&s, "-+ #0", -1, "", 'c');
  make_format(fmt, &s);
  sprintf(value_text, "%c", v);
  return CALL(fmt, s, v);
}

static int fuzz_pointer(void) {
  const uintptr_t high = (uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1);
  const uintptr_t pool[] = {0, 1, 0x1234, 0xdeadbeef, high | 0x1234, ~(uintptr_t)0};
  struct spec s;
  char fmt[128];
  void *v = (void *)pool[rnd(6)];

  make_spec(&s, "-", -1, "", 'p');
  make_format(fmt, &s);
  sprintf(value_text, "%p", v);
  return CALL(fmt, s, v);
}

static double float_value(void) {
  static const double special[] = {0.0, 1.0, 0.5, 0.1, 9.5, 99.5, 999999.5,
      DBL_MAX, DBL_MIN, 5e-324, 1e22, 1e23, 123456789.0, 4503599627370497.0};
  double v;

  switch (rnd(10)) {
    case 0: /* any magnitude, subnormals too */
      v = pow(10, -320 + 628 * rnd01()) * (0.5 + rnd01());
      break;
    case 1:
    case 2:
      v = pow(10, -8 + 16 * rnd01());
      break;
    case 3: /* exact ties for many precisions */
      v = (double)rnd(100000) / (double)(1u << rnd(20));
      break;
    case 4:
      v = floor(pow(10, 12 * rnd01())) + 0.5;
      break;
    case 5: /* just below and above a power of ten, where rounding carries */
      v = pow(10, (double)((int)rnd(80) - 40)) * (1.0 - 1e-7 * rnd01());
      break;
    case 6:
      v = pow(10, (double)((int)rnd(80) - 40)) * (1.0 + 1e-7 * rnd01());
      break;
    case 7:
      v = special[rnd(sizeof special / sizeof *special)];
      break;
    case 8:
      v = rnd(3) == 0 ? (rnd(2) ? INFINITY : NAN) : 0.0;
      break;
    default: /* random digits of the mantissa */
      v = (double)(long long)(rnd64() >> 12) * pow(2, -(int)rnd(60));
      break;
  }
  if (v > DBL_MAX) {
    v = DBL_MAX;
  }
  return v == v && rnd(2) ? -v : v; /* the sign of NaN is not compared */
}

static int fuzz_float(void) {
  static const char conversions[] = "eEfFgG";
  char conversion = conversions[rnd(6)];
  double v = float_value();
  struct spec s;
  char fmt[128];

  /* the # flag of %g is left out, glibc drops digits when rounding carries */
  make_spec(&s, conversion == 'g' || conversion == 'G' ? "-+ 0" : "-+ #0", 29, "", conversion);
  make_format(fmt, &s);
  sprintf(value_text, "%.17g", v);
  return CALL(fmt, s, v);
}


int main(int argc, char **argv) {
  unsigned long long seed = argc > 1 ? strtoull(argv[1], NULL, 0) : 1;
  unsigned long iterations = argc > 2 ? strtoul(argv[2], NULL, 0) : 200000;
  unsigned long i;

  rng_state = seed;
  for (i = 0; i < iterations; i++) {
    unsigned kind = rnd(100);
    if (kind < 30) {
      fuzz_integer();
    } else if (kind < 80) {
      fuzz_float();
    } else if (kind < 90) {
      fuzz_string();
    } else if (kind < 95) {
      fuzz_char();
    } else {
      fuzz_pointer();
    }
  }

  printf("seed %llu: %lu formats checked, %lu mismatches\n", seed, checked, failed);
  return failed != 0;
}
