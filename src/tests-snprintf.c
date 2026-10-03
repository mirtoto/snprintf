// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com
#include <limits.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "minunit.h"

#include "snprintf.h"

#include "tests-snprintf.h"


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
// because of MinUnit
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif


static char msg[32] = {0, };


/**
 * The number of characters written into msg, not counting the terminating '\0',
 * or sizeof(msg) when msg is not terminated at all.
 */
static int written_length(void) {
	int i;
	for (i = 0; i < (int)sizeof(msg); i++) {
		if (msg[i] == '\0') {
			return i;
		}
	}
	return (int)sizeof(msg);
}


/* ret is the length of the whole output, which can be longer than the buffer,
   so the terminating '\0' is looked for in the buffer and not at msg[ret]. */
#define TEST(ret_expected, msg_expected, ret) { 		\
	mu_assert_int_eq((ret_expected), ret); 				\
	if ((ret) >= 0) { 									\
		mu_check(written_length() < (int)sizeof(msg)); 	\
		mu_assert_string_eq((msg_expected), msg); 		\
	} 													\
}


/* The length the function returns for a whole output of @p whole characters of
   which @p written fit into the buffer: the whole length, or only the written
   part in the legacy mode of SNPRINTF_LEGACY_LENGTH. */
#ifdef SNPRINTF_LEGACY_LENGTH
#define RETURNED(whole, written) (written)
#else
#define RETURNED(whole, written) (whole)
#endif


#if __GNUC__ >= 7
#pragma GCC diagnostic push
// These tests intentionally exercise truncation; suppress GCC's format warnings.
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wformat="
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

MU_TEST(test_buffer_null) {
	int ret = 0;
	for (char *pmsg = NULL;;) {
		ret = snprintf(pmsg, (size_t)ret,
			"%s %s%c : %d", "Hello", "World", '!', 2020);
		mu_check(ret == 19);
		if (pmsg != NULL) {
			strncpy(msg, pmsg, sizeof(msg));
			free(pmsg);
			break;
		} else if (ret > 0) {
			// allocate buffer for output string (+1 because of '\0')
			pmsg = malloc((size_t)++ret);
		}
	}
	TEST(19, "Hello World! : 2020", ret);
}

MU_TEST(test_buffer_length_0) {
	int ret = snprintf(msg, 0, "%d", 123);
	TEST(-1, NULL, ret);
}

MU_TEST(test_buffer_length_1) {
	int ret = snprintf(msg, 1, "%d", 123);
	TEST(RETURNED(3, 0), "", ret);
}

MU_TEST(test_buffer_length_2) {
	int ret = snprintf(msg, 2, "%d", 123);
	TEST(RETURNED(3, 1), "1", ret);
}

MU_TEST(test_buffer_length_3) {
	int ret = snprintf(msg, 3, "%d", 123);
	TEST(RETURNED(3, 2), "12", ret);
}

#ifdef __clang__
#pragma clang diagnostic push
// These tests intentionally use malformed formats; suppress Clang's format warning.
#pragma clang diagnostic ignored "-Wformat"
#endif

#ifdef SNPRINTF_STRICT
MU_TEST(test_wrong_format_no_type) {
	int ret = snprintf(msg, sizeof(msg), "%d%", 123);
	mu_assert_int_eq(-1, ret);
}

MU_TEST(test_wrong_format_unsupported_type) {
	int ret = snprintf(msg, sizeof(msg), "%d%v", 123);
	mu_assert_int_eq(-1, ret);

	ret = snprintf(msg, 2, "A%v", 123);
	mu_assert_int_eq(-1, ret);
}

MU_TEST(test_plus_flag_and_left_align) {
	int ret = snprintf(msg, sizeof(msg), "%+-10d", 123);
	TEST(10, "+123      ", ret);

	ret = snprintf(msg, sizeof(msg), "%+10d", 123);
	TEST(10, "      +123", ret);

	ret = snprintf(msg, sizeof(msg), "% 10d", 123);
	TEST(10, "       123", ret);

	ret = snprintf(msg, sizeof(msg), "% -10d", 123);
	TEST(10, " 123      ", ret);
}

MU_TEST(test_strict_mode_rejects_malformed_specifier) {
	int ret = snprintf(msg, sizeof(msg), "%q", 123);
	mu_assert_int_eq(-1, ret);

	ret = snprintf(msg, sizeof(msg), "%+", 123);
	mu_assert_int_eq(-1, ret);

	ret = snprintf(msg, sizeof(msg), "%0", 123);
	mu_assert_int_eq(-1, ret);

	ret = snprintf(msg, sizeof(msg), "%#s", "text");
	mu_assert_int_eq(-1, ret);
}
#else
MU_TEST(test_wrong_format_no_type) {
	int ret = snprintf(msg, sizeof(msg), "%d%", 123);
	TEST(4, "123%", ret);
}

MU_TEST(test_wrong_format_unsupported_type) {
	int ret = snprintf(msg, sizeof(msg), "%d%v", 123);
	TEST(4, "123%", ret);
}

MU_TEST(test_plus_flag_and_left_align) {
	int ret = snprintf(msg, sizeof(msg), "%+-10d", 123);
	TEST(10, "+123      ", ret);

	ret = snprintf(msg, sizeof(msg), "%+10d", 123);
	TEST(10, "      +123", ret);

	ret = snprintf(msg, sizeof(msg), "% 10d", 123);
	TEST(10, "       123", ret);

	ret = snprintf(msg, sizeof(msg), "% -10d", 123);
	TEST(10, " 123      ", ret);
}

MU_TEST(test_malformed_format_standard_like) {
	int ret = snprintf(msg, sizeof(msg), "%q", 123);
	TEST(1, "%", ret);

	ret = snprintf(msg, sizeof(msg), "%+", 123);
	TEST(1, "%", ret);

	ret = snprintf(msg, sizeof(msg), "%0", 123);
	TEST(1, "%", ret);

	ret = snprintf(msg, sizeof(msg), "%#", 123);
	TEST(1, "%", ret);

	ret = snprintf(msg, sizeof(msg), "%.", 123);
	TEST(1, "%", ret);

	ret = snprintf(msg, sizeof(msg), "%d %", 123);
	TEST(5, "123 %", ret);

	ret = snprintf(msg, sizeof(msg), "%hh", 123);
	TEST(1, "%", ret);

	ret = snprintf(msg, sizeof(msg), "%ll", 123);
	TEST(1, "%", ret);

	ret = snprintf(msg, sizeof(msg), "%..d", 123);
	TEST(3, "123", ret);
}
#endif

MU_TEST(test_int_dec_width_31_and_align_left) {
	int ret = snprintf(msg, sizeof(msg), "%-5d|%-05d", 123, 123);
	TEST(11, "123  |123  ", ret);
}

MU_TEST(test_long_long_unsigned_sign_flags) {
	unsigned long long u = 1234567890;
	snprintf(msg, sizeof(msg), "%+llu % llu", u, u);
	mu_check(strcmp(msg, "1234567890 1234567890") == 0);
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif

MU_TEST(test_char_dec) {
	int ret = snprintf(msg, sizeof(msg), "%hhd %hhd% hhd %hhu",
		(char)0, (char)123, (char)123, (char)123);
	TEST(13, "0 123 123 123", ret);
}

MU_TEST(test_char_dec_min_and_max) {
	snprintf(msg, sizeof(msg), "%hhd", (char)CHAR_MIN);
	mu_check(atoll(msg) == CHAR_MIN);
	snprintf(msg, sizeof(msg), "%hhd", (char)CHAR_MAX);
	mu_check(atoll(msg) == CHAR_MAX);
}

MU_TEST(test_char_dec_negative) {
	int ret = snprintf(msg, sizeof(msg), "%hhd% hhd %hhu",
		(char)-123, (char)-123, (char)-123);
	TEST(12, "-123-123 133", ret);
}

MU_TEST(test_short_dec) {
	int ret = snprintf(msg, sizeof(msg), "%hd %hd% hd %hu",
		(short)0, (short)1230, (short)1230, (short)1230);
	TEST(16, "0 1230 1230 1230", ret);
}

MU_TEST(test_short_dec_min_and_max) {
	snprintf(msg, sizeof(msg), "%hd", (short)SHRT_MIN);
	mu_check(atoll(msg) == SHRT_MIN);
	snprintf(msg, sizeof(msg), "%hd", (short)SHRT_MAX);
	mu_check(atoll(msg) == SHRT_MAX);
}

MU_TEST(test_short_dec_negative) {
	int ret = snprintf(msg, sizeof(msg), "%hd% hd %hu",
		(short)-1230, (short)-1230, (short)-1230);
	TEST(16, "-1230-1230 64306", ret);
}

MU_TEST(test_int_dec) {
	int ret = snprintf(msg, sizeof(msg), "%d %d% d %u", 0, 123, 123, 123);
	TEST(13, "0 123 123 123", ret);
}

MU_TEST(test_int_dec_min_and_max) {
	snprintf(msg, sizeof(msg), "%d", INT_MIN);
	mu_check(atoll(msg) == INT_MIN);
	snprintf(msg, sizeof(msg), "%d", INT_MAX);
	mu_check(atoll(msg) == INT_MAX);
}

MU_TEST(test_int_dec_negative) {
	int ret = snprintf(msg, sizeof(msg), "%d% d %u", -123, -123, -123);
	TEST(19, "-123-123 4294967173", ret);
}

MU_TEST(test_int_dec_width_10) {
	int ret = snprintf(msg, sizeof(msg), "%10d", -123);
	TEST(10, "      -123", ret);
}

MU_TEST(test_int_dec_width_31_and_0_padded) {
	int ret = snprintf(msg, sizeof(msg), "%031d", 123);
	TEST(31, "0000000000000000000000000000123", ret);
}

MU_TEST(test_int_dec_width_2) {
	int ret = snprintf(msg, sizeof(msg), "%2d", 123);
	TEST(3, "123", ret);
}

MU_TEST(test_int_dec_width_20_precision_10) {
	int ret = snprintf(msg, sizeof(msg), "%20.10d", 123);
	TEST(20, "          0000000123", ret);
}

MU_TEST(test_int_dec_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%.0d %.0d", 123, 0);
	TEST(4, "123 ", ret);
}

MU_TEST(test_int_dec_width_as_parameter) {
	int ret = snprintf(msg, sizeof(msg), "%*d", 5, 123);
	TEST(5, "  123", ret);
}

MU_TEST(test_int_dynamic_precision) {
	int ret = snprintf(msg, sizeof(msg), "%.*d %*.*x", 4, 12, 8, 4, 42u);
	TEST(13, "0012     002a", ret);

	ret = snprintf(msg, sizeof(msg), "%.*d", -1, 12);
	TEST(2, "12", ret);
}

MU_TEST(test_int_dec_random) {
    time_t tt;
	srand((unsigned int)time(&tt));
	int d = rand();
	snprintf(msg, sizeof(msg), "%d", d);
	mu_check(atoll(msg) == d);
}

MU_TEST(test_int_i_length_modifiers) {
	int ret = snprintf(msg, sizeof(msg), "%i %li %lli", -12, 123456l,
		-123456789ll);
	TEST(21, "-12 123456 -123456789", ret);
}

MU_TEST(test_int_hex) {
	int ret = snprintf(msg, sizeof(msg), "%x %x %#x", 0, 123, 123);
	TEST(9, "0 7b 0x7b", ret);
}

MU_TEST(test_int_hex_uppercase) {
	int ret = snprintf(msg, sizeof(msg), "%X %#X", 123, 123);
	TEST(7, "7B 0X7B", ret);
}

MU_TEST(test_int_hex_negative) {
	const char expected[] = "ffffffffffffff85";
	int x = -123;
	int ret = snprintf(msg, sizeof(msg), "%x", x);
	TEST(sizeof(x) * 2, expected + strlen(expected) - sizeof(x) * 2, ret);
}

MU_TEST(test_int_hex_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%#.0x %#.0x", 123, 0);
	TEST(5, "0x7b ", ret);
}

MU_TEST(test_octal_alternative_form) {
	const char expected[] = "0 0 010 0     0 00000 0001";
	int ret = snprintf(msg, sizeof(msg), "%o %#o %#o %#.0o %#5o %#05o %#.4o",
		0u, 0u, 8u, 0u, 0u, 0u, 1u);
	TEST((int)strlen(expected), expected, ret);
}

MU_TEST(test_unsigned_long_and_octal_lengths) {
	int ret = snprintf(msg, sizeof(msg), "%lu %llu %lo %llo",
		123456ul, 123456789ull, 64ul, 64ull);
	TEST(24, "123456 123456789 100 100", ret);
}

MU_TEST(test_long_dec) {
	int ret = snprintf(msg, sizeof(msg), "%ld", 123000l);
	TEST(6, "123000", ret);
}

MU_TEST(test_long_hex) {
	int ret = snprintf(msg, sizeof(msg), "%lx %lX", 123000l, 123000l);
	TEST(11, "1e078 1E078", ret);
}

MU_TEST(test_long_hex_alternative) {
	int ret = snprintf(msg, sizeof(msg), "%#lx %#lX", 123000l, 123000l);
	TEST(15, "0x1e078 0X1E078", ret);
}

MU_TEST(test_long_hex_width_as_type) {
	const char expected[] = "0000000000000000000000000001e078";
	long x = 123000l;
	int ret = snprintf(msg, sizeof(msg), "%0*lx", (int)(sizeof(x) * 2), x);
	TEST(sizeof(x) * 2, expected + strlen(expected) - sizeof(x) * 2, ret);
}

MU_TEST(test_long_long_dec) {
	int ret = snprintf(msg, sizeof(msg), "%lld", 123000000000ll);
	TEST(12, "123000000000", ret);
}

MU_TEST(test_long_long_dec_min) {
	long long d = LLONG_MIN;
	snprintf(msg, sizeof(msg), "%lld", d);
	mu_check(atoll(msg) == d);
}

MU_TEST(test_long_long_dec_max) {
	long long d = LLONG_MAX;
	snprintf(msg, sizeof(msg), "%lld", d);
	mu_check(atoll(msg) == d);
}

MU_TEST(test_long_long_unsigned_max) {
	unsigned long long u = ULLONG_MAX;
	snprintf(msg, sizeof(msg), "%llu", u);
	char *ptr = NULL;
	mu_check(strtoull(msg, &ptr, 10) == u);
}

MU_TEST(test_long_long_zero_pad_negative) {
	int ret = snprintf(msg, sizeof(msg), "%020lld", -1LL);
	TEST(20, "-0000000000000000001", ret);
}

MU_TEST(test_long_long_hex_zero_pad_alternative) {
	int ret = snprintf(msg, sizeof(msg), "%#020llx", 123ULL);
	TEST(20, "0x00000000000000007b", ret);
}

MU_TEST(test_long_long_hex) {
	int ret = snprintf(msg, sizeof(msg), "%llx %llX", 123000000000ll, 123000000000ll);
	TEST(21, "1ca35f0e00 1CA35F0E00", ret);
}

MU_TEST(test_long_long_hex_alternative) {
	int ret = snprintf(msg, sizeof(msg), "%#llx %#llX", 123000000000ll, 123000000000ll);
	TEST(25, "0x1ca35f0e00 0X1CA35F0E00", ret);
}

MU_TEST(test_long_long_hex_width_as_type) {
	const char expected[] = "00000000000000000000001ca35f0e00";
	long long x = 123000000000ll;
	int ret = snprintf(msg, sizeof(msg), "%0*llx", (int)(sizeof(x) * 2), x);
	TEST(sizeof(x) * 2, expected + strlen(expected) - sizeof(x) * 2, ret);
}

MU_TEST(test_long_long_hex_max) {
	char expected[] = "ffffffffffffffffffffffffffffffff";
	unsigned long long x = ULLONG_MAX;
	int ret = snprintf(msg, sizeof(msg), "%llx", x);
	expected[sizeof(x) * 2] = '\0';
	TEST(sizeof(x) * 2, expected, ret);
}

MU_TEST(test_double_f) {
	int ret = snprintf(msg, sizeof(msg), "%f %f %F",
		0.0, 123.0, 123.0 + 1.0 / 3);
	TEST(30, "0.000000 123.000000 123.333333", ret);
}

MU_TEST(test_double_f_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%.0f %.0f %.0F",
		0.0, 123.0, 123.0 + 1.0 / 3);
	TEST(9, "0 123 123", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f %.0f %.0f %.0f",
		2.4, 2.6, -2.4, -2.6);
	TEST(9, "2 3 -2 -3", ret);
}

MU_TEST(test_double_f_precision_2_3) {
	int ret = snprintf(msg, sizeof(msg), "%2.3f %2.3f %2.3F",
		0.0, 123.0, 123.0 + 1.0 / 3);
	TEST(21, "0.000 123.000 123.333", ret);
}

MU_TEST(test_double_e_zero) {
	int ret = snprintf(msg, sizeof(msg), "%e %E", 0.0, 0.0);
	TEST(25, "0.000000e+00 0.000000E+00", ret);
}

MU_TEST(test_double_e) {
	int ret = snprintf(msg, sizeof(msg), "%e %E",
		123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(25, "1.233333e+02 1.233333E+02", ret);
}

MU_TEST(test_double_e_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%.0e %.0E %.0e %.0E",
		0.0, 0.0, 123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(23, "0e+00 0E+00 1e+02 1E+02", ret);
}

MU_TEST(test_double_e_precision_2_3) {
	int ret = snprintf(msg, sizeof(msg), "%2.3e %2.3E",
		123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(19, "1.233e+02 1.233E+02", ret);
}

MU_TEST(test_double_g) {
	int ret = snprintf(msg, sizeof(msg), "%g %G",
		123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(15, "123.333 123.333", ret);
}

MU_TEST(test_double_g_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%.0g %.0G %.0g %.0G",
		0.0, 0.0, 1.0 / 123000000.0, 1.0 / 123000000.0);
	TEST(15, "0 0 8e-09 8E-09", ret);
}

MU_TEST(test_double_g_precision_2_7) {
	int ret = snprintf(msg, sizeof(msg), "%2.7g %2.7G",
		1.0 / 123000000.0, 1.0 / 123000000.0);
	TEST(25, "8.130081e-09 8.130081E-09", ret);
}

MU_TEST(test_double_g_alternate_form_and_significant_precision) {
	int ret = snprintf(msg, sizeof(msg), "%#.6g %#.6G", 1.25, 1.25);
	TEST(15, "1.25000 1.25000", ret);

	ret = snprintf(msg, sizeof(msg), "%.3g %.3G", 123.456, 123.456);
	TEST(7, "123 123", ret);

	ret = snprintf(msg, sizeof(msg), "%#.3g %#.3G", 12000.0, 12000.0);
	TEST(17, "1.20e+04 1.20E+04", ret);

	ret = snprintf(msg, sizeof(msg), "%*.*g", 8, 3, 123.456);
	TEST(8, "     123", ret);

	ret = snprintf(msg, sizeof(msg), "%*.*g", 8, -1, 1.25);
	TEST(8, "    1.25", ret);
}

MU_TEST(test_double_negative_and_sign_flags) {
	int ret = snprintf(msg, sizeof(msg), "%f %.2e", -1.25, -12.5);
	TEST(19, "-1.250000 -1.25e+01", ret);

	ret = snprintf(msg, sizeof(msg), "%+f % f", 2.25, 2.25);
	TEST(19, "+2.250000  2.250000", ret);
}

MU_TEST(test_double_g_boundaries_and_trim) {
	int ret = snprintf(msg, sizeof(msg), "%g %g %g %g",
		0.0001, 0.00001, 1.0, 1000000.0);
	TEST(20, "0.0001 1e-05 1 1e+06", ret);

	ret = snprintf(msg, sizeof(msg), "%.3g %.1g", 999.9, 0.00009999);
	TEST(12, "1e+03 0.0001", ret);
}

MU_TEST(test_double_e_rounds_mantissa) {
	int ret = snprintf(msg, sizeof(msg), "%.6e", 9.9999996);
	TEST(12, "1.000000e+01", ret);
}

MU_TEST(test_double_extreme_exponents_and_precision) {
	int ret = snprintf(msg, sizeof(msg), "%e %e", 1e-300, 1e300);
	TEST(27, "1.000000e-300 1.000000e+300", ret);

	ret = snprintf(msg, sizeof(msg), "%.6f", 1e-8);
	TEST(8, "0.000000", ret);

	ret = snprintf(msg, sizeof(msg), "%.20f", 1e-20);
	TEST(22, "0.00000000000000000001", ret);
}

MU_TEST(test_double_finite_limits) {
	int ret = snprintf(msg, sizeof(msg), "%.6e", DBL_MIN);
	TEST(13, "2.225074e-308", ret);

	ret = snprintf(msg, sizeof(msg), "%.6e", DBL_MAX);
	TEST(13, "1.797693e+308", ret);

	double min_subnormal;
	uint64_t subnormal_bits = 1ULL;
	memcpy(&min_subnormal, &subnormal_bits, sizeof(min_subnormal));
	ret = snprintf(msg, sizeof(msg), "%.6e", min_subnormal);
	TEST(13, "4.940656e-324", ret);

	ret = snprintf(msg, sizeof(msg), "%.6g", min_subnormal);
	TEST(12, "4.94066e-324", ret);
}

MU_TEST(test_double_large_fixed_truncates_safely) {
	struct {
		char output[8];
		char canary;
	} buffer;
	buffer.canary = 'X';

	int ret = snprintf(buffer.output, sizeof(buffer.output), "%.0f", 1e100);
	mu_assert_int_eq(RETURNED(101, 7), ret); /* the length of 1e100, not the size of the buffer */
	mu_check(buffer.output[0] != '\0');
	mu_check(buffer.output[7] == '\0');
	mu_assert_int_eq('X', buffer.canary);
}

MU_TEST(test_special_float_values) {
	int ret = snprintf(msg, sizeof(msg), "%f %E %G", INFINITY, -INFINITY, NAN);
	TEST(12, "inf -INF NAN", ret);

	ret = snprintf(msg, sizeof(msg), "%+8f", INFINITY);
	TEST(8, "    +inf", ret);
	ret = snprintf(msg, sizeof(msg), "%-8F", -INFINITY);
	TEST(8, "-INF    ", ret);
}

MU_TEST(test_extreme_format_width_and_precision) {
	struct {
		char output[8];
		char canary;
	} buffer;
	char expected[32] = "1.25";
	memset(expected + 4, '0', 27);
	expected[31] = '\0';
	buffer.canary = 'X';

	const char *wide_format = "%2147483648s";
	int ret = snprintf(buffer.output, sizeof(buffer.output), wide_format, "x");
	// a width that does not fit into an int is not defined by the C standard,
	// the saturated width is counted and the result is cut to INT_MAX
	mu_assert_int_eq(RETURNED(INT_MAX, 7), ret);
	for (size_t i = 0; i < sizeof(buffer.output) - 1; i++) {
		mu_check(buffer.output[i] == ' ');
	}
	mu_check(buffer.output[7] == '\0');
	mu_assert_int_eq('X', buffer.canary);

	ret = snprintf(buffer.output, sizeof(buffer.output), "%*s", INT_MAX, "x");
	mu_assert_int_eq(RETURNED(INT_MAX, 7), ret);
	for (size_t i = 0; i < sizeof(buffer.output) - 1; i++) {
		mu_check(buffer.output[i] == ' ');
	}
	mu_check(buffer.output[7] == '\0');
	mu_assert_int_eq('X', buffer.canary);

	// a NULL buffer is never full, so the whole length is reported in both modes
	ret = snprintf(NULL, 0, "%*s", INT_MAX, "x");
	mu_assert_int_eq(INT_MAX, ret);

	const char *dynamic_width_format = "%*s";
	ret = snprintf(buffer.output, sizeof(buffer.output), dynamic_width_format,
		INT_MIN, "x");
	mu_assert_int_eq(RETURNED(INT_MAX, 7), ret); /* a negative width from a star is the '-' flag */
	mu_check(buffer.output[0] == 'x');
	for (size_t i = 1; i < sizeof(buffer.output) - 1; i++) {
		mu_check(buffer.output[i] == ' ');
	}
	mu_check(buffer.output[7] == '\0');
	mu_assert_int_eq('X', buffer.canary);

	const char *wide_precision_format = "%.2147483648f";
	ret = snprintf(msg, sizeof(msg), wide_precision_format, 1.25);
	TEST(31, expected, ret);
	ret = snprintf(msg, sizeof(msg), "%.*f", INT_MIN, 1.25);
	TEST(8, "1.250000", ret);
}

MU_TEST(test_double_null_buffer_and_truncation) {
	const char *format = "%.2f %.2e";
	char output[32];
	int required = snprintf(NULL, 0, format, 1.25, 12.5);
	int written = snprintf(output, sizeof(output), format, 1.25, 12.5);
	mu_assert_int_eq(13, required);
	mu_assert_int_eq(required, written);
	mu_assert_string_eq("1.25 1.25e+01", output);

	char small_output[5];
	int ret = snprintf(small_output, sizeof(small_output), "%.2f", 123.45);
	mu_assert_int_eq(RETURNED(6, 4), ret); /* "123.45" does not fit into 4 characters */
	mu_assert_string_eq("123.", small_output);
}

MU_TEST(test_double_fraction_buffer_boundary) {
	char expected[32] = "1.25";
	memset(expected + 4, '0', 27);
	expected[31] = '\0';
	int ret = snprintf(msg, sizeof(msg), "%.29f", 1.25);
	TEST(31, expected, ret);

	expected[0] = '0';
	expected[1] = '.';
	memset(expected + 2, '0', 27);
	expected[29] = '1';
	expected[30] = '0';
	expected[31] = '\0';
	ret = snprintf(msg, sizeof(msg), "%.29f", 1e-28);
	TEST(31, expected, ret);

	ret = snprintf(msg, sizeof(msg), "%.7f", 0.9999999);
	TEST(9, "0.9999999", ret);
}

MU_TEST(test_double_width_and_zero_padding) {
	int ret = snprintf(msg, sizeof(msg), "%+012.2f", 12.5);
	TEST(12, "+00000012.50", ret);

	ret = snprintf(msg, sizeof(msg), "% 012.2f", 12.5);
	TEST(12, " 00000012.50", ret);

	ret = snprintf(msg, sizeof(msg), "%012.2f", -12.5);
	TEST(12, "-00000012.50", ret);

	ret = snprintf(msg, sizeof(msg), "%-12.2e", 12.5);
	TEST(12, "1.25e+01    ", ret);

	ret = snprintf(msg, sizeof(msg), "%+012.2e", 12.5);
	TEST(12, "+0001.25e+01", ret);

	ret = snprintf(msg, sizeof(msg), "%012.2e", -12.5);
	TEST(12, "-0001.25e+01", ret);

	ret = snprintf(msg, sizeof(msg), "%10g|%-10g", 1.0, 1.0);
	TEST(21, "         1|1         ", ret);

	ret = snprintf(msg, sizeof(msg), "%-12.2g", 1e-5);
	TEST(12, "1e-05       ", ret);
}

MU_TEST(test_double_dynamic_width_and_precision) {
	int ret = snprintf(msg, sizeof(msg), "%*.*f", 8, 2, 1.25);
	TEST(8, "    1.25", ret);

	ret = snprintf(msg, sizeof(msg), "%0*.*f", 10, 2, 1.25);
	TEST(10, "0000001.25", ret);

	ret = snprintf(msg, sizeof(msg), "%*.*e", 12, 2, -12.5);
	TEST(12, "   -1.25e+01", ret);

	ret = snprintf(msg, sizeof(msg), "%.*f", INT_MIN, 1.25);
	TEST(8, "1.250000", ret);
}

MU_TEST(test_double_alternate_form_zero_precision_width) {
	int ret = snprintf(msg, sizeof(msg), "%#12.0f", 3.0);
	TEST(12, "          3.", ret);

	ret = snprintf(msg, sizeof(msg), "%#12.0e", 3.0);
	TEST(12, "      3.e+00", ret);
}

MU_TEST(test_string_null_pointer) {
	const char *str = NULL;
	int ret = snprintf(msg, sizeof(msg), "%s", str);
	TEST(6, "(null)", ret);
}

MU_TEST(test_string) {
	int ret = snprintf(msg, sizeof(msg), "%s", "Hello");
	TEST(5, "Hello", ret);
}

MU_TEST(test_string_empty) {
	int ret = snprintf(msg, sizeof(msg), "%s", "");
	TEST(0, "", ret);
}

MU_TEST(test_string_width_20) {
	int ret = snprintf(msg, sizeof(msg), "%20s", "Hello");
	TEST(20, "               Hello", ret);
}

MU_TEST(test_string_width_20_and_align_left) {
	int ret = snprintf(msg, sizeof(msg), "%-20s", "Hello");
	TEST(20, "Hello               ", ret);
}

MU_TEST(test_string_width_20_precision_2) {
	int ret = snprintf(msg, sizeof(msg), "%20.2s", "Hello");
	TEST(20, "                  He", ret);
}

MU_TEST(test_string_width_20_precision_20) {
	int ret = snprintf(msg, sizeof(msg), "%20.20s", "Hello");
	TEST(20, "               Hello", ret);
}

MU_TEST(test_string_with_less_than_input) {
	int ret = snprintf(msg, sizeof(msg), "%3s", "Hello");
	TEST(5, "Hello", ret);
}

MU_TEST(test_string_with_less_than_input_precision_equal_width) {
	int ret = snprintf(msg, sizeof(msg), "%3.3s", "Hello");
	TEST(3, "Hel", ret);
}

MU_TEST(test_string_width_as_parameter) {
	// a lone '.' means precision 0, so only the padding is printed
	int ret = snprintf(msg, sizeof(msg), "%-*.s%*.s!", 10, "Hello", 10, "World");
	TEST(21, "                    !", ret);
}

MU_TEST(test_string_precision_as_parameter) {
	int ret = snprintf(msg, sizeof(msg), "%-.*s%.*s!", 10, "Hello", 10, "World");
	TEST(11, "HelloWorld!", ret);
}

MU_TEST(test_string_precision_nonterminated_span) {
	const char span[1] = {'Q'};
	int ret = snprintf(msg, sizeof(msg), "%.1s", span);
	TEST(1, "Q", ret);
}

MU_TEST(test_string_width_and_precision_as_parameter) {
	int ret = snprintf(msg, sizeof(msg), "%-*.*s%*.*s!",
		10, 10, "Hello", 10, 10, "World");
	TEST(21, "Hello          World!", ret);
}

MU_TEST(test_string_width_as_parameter_negative) {
	int ret = snprintf(msg, sizeof(msg), "%*s", -20, "Hello World!");
	TEST(20, "Hello World!        ", ret);
}

MU_TEST(test_string_too_long) {
	const char *str = "This is very long message and it is much longer than buffer!";
	int ret = snprintf(msg, sizeof(msg), "%s", str);
	TEST(RETURNED((int)strlen(str), (int)sizeof(msg) - 1), "This is very long message and i", ret);
}

MU_TEST(test_strings) {
	int ret = snprintf(msg, sizeof(msg), "%s %s%c", "Hello", "World", '!');
	TEST(12, "Hello World!", ret);
}

MU_TEST(test_chars) {
	int ret = snprintf(msg, sizeof(msg), "%c%c%c%c%c", 'H', 'e', 'l', 'l', 'o');
	TEST(5, "Hello", ret);
}

MU_TEST(test_char_width_and_alignment) {
	int ret = snprintf(msg, sizeof(msg), "%4c %-4c", 'A', 'B');
	TEST(9, "   A B   ", ret);

	ret = snprintf(msg, sizeof(msg), "%*c", 3, 'C');
	TEST(3, "  C", ret);

	ret = snprintf(msg, sizeof(msg), "%*c", -3, 'D');
	TEST(3, "D  ", ret);
}

MU_TEST(test_pointer_null) {
	int ret = snprintf(msg, sizeof(msg), "%p", (void *)0);
	TEST(5, "(nil)", ret);
}

MU_TEST(test_pointer) {
#if UINTPTR_MAX > 0xffffffffu
    int ret = snprintf(msg, sizeof(msg), "%p", (void *)0x12345678aabbccdd);
    TEST(18, "0x12345678aabbccdd", ret);
#else
    // the high bit must not be sign-extended to 64 bits
    int ret = snprintf(msg, sizeof(msg), "%p", (void *)0x9abcdef0u);
    TEST(10, "0x9abcdef0", ret);
#endif
}

MU_TEST(test_pointer_width) {
#if UINTPTR_MAX > 0xffffffffu
	int ret = snprintf(msg, sizeof(msg), "%*p", 20, (void *)0x12345678aabbccdd);
	TEST(20, "  0x12345678aabbccdd", ret);
#else
	// the constant above is truncated to 32 bits on this target
	int ret = snprintf(msg, sizeof(msg), "%*p", 20, (void *)0x9abcdef0u);
	TEST(20, "          0x9abcdef0", ret);
#endif
}

MU_TEST(test_percent) {
	const char *str = "%%%%% Hello World! %%%%%";
	int ret = snprintf(msg, sizeof(msg), "%%%%%%%%%% Hello World! %%%%%%%%%%");
	TEST((int)strlen(str), str, ret);
}

#if defined(__clang__)
#pragma clang diagnostic push
// These tests intentionally use flags that the compiler's format check dislikes.
#pragma clang diagnostic ignored "-Wformat"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif

MU_TEST(test_unsigned_long_long_max) {
	int ret = snprintf(msg, sizeof(msg), "%llu", ULLONG_MAX);
	mu_check(ret > 0);
	mu_check(msg[0] != '-');
	mu_check(strtoull(msg, NULL, 10) == ULLONG_MAX);

	ret = snprintf(msg, sizeof(msg), "%llu", 9223372036854775808ULL);
	TEST(19, "9223372036854775808", ret);
}

MU_TEST(test_unsigned_long_max) {
	int ret = snprintf(msg, sizeof(msg), "%lu", ULONG_MAX);
	mu_check(ret > 0);
	mu_check(msg[0] != '-');
	mu_check(strtoul(msg, NULL, 10) == ULONG_MAX);
}

MU_TEST(test_unsigned_ignores_plus_and_space_flags) {
	int ret = snprintf(msg, sizeof(msg), "%+u % u %+x", 5u, 5u, 5u);
	TEST(5, "5 5 5", ret);

	ret = snprintf(msg, sizeof(msg), "%+d % d", 5, 5);
	TEST(5, "+5  5", ret);
}

MU_TEST(test_pointer_dynamic_width) {
	void *p = (void *)0x1234;
	int ret = snprintf(msg, sizeof(msg), "[%*p]%d", 12, p, 7);
	TEST(15, "[      0x1234]7", ret);

	ret = snprintf(msg, sizeof(msg), "[%-*p]%d", 12, p, 7);
	TEST(15, "[0x1234      ]7", ret);

	ret = snprintf(msg, sizeof(msg), "[%*p]%d", -12, p, 7);
	TEST(15, "[0x1234      ]7", ret);
}

MU_TEST(test_left_align_ignores_zero_flag) {
	int ret = snprintf(msg, sizeof(msg), "%-05d|%0*d|%-05x|%-08.3f",
		12, -5, 12, 255u, 1.5);
	TEST(26, "12   |12   |ff   |1.500   ", ret);
}

MU_TEST(test_lone_dot_is_precision_zero) {
	int ret = snprintf(msg, sizeof(msg), "%.s|%.d|%.d|%.f|%.x",
		"Hello", 0, 7, 1.5, 0u);
	TEST(6, "||7|2|", ret);
}

MU_TEST(test_hex_alternative_form_of_zero) {
	int ret = snprintf(msg, sizeof(msg), "%#x %#X %#llx %#o",
		0u, 0u, 0ull, 0u);
	TEST(7, "0 0 0 0", ret);
}

MU_TEST(test_zero_flag_ignored_with_integer_precision) {
	int ret = snprintf(msg, sizeof(msg), "%08.3d|%08.3u|%08.3x",
		12, 12u, 255u);
	TEST(26, "     012|     012|     0ff", ret);

	ret = snprintf(msg, sizeof(msg), "%08.3d", -12);
	TEST(8, "    -012", ret);
}

#ifndef SNPRINTF_STRICT
MU_TEST(test_zero_flag_on_string_pads_with_blanks) {
	int ret = snprintf(msg, sizeof(msg), "%05s|%05.1s", "ab", "xyz");
	TEST(11, "   ab|    x", ret);
}
#endif

MU_TEST(test_size_ptrdiff_and_intmax_lengths) {
	int ret = snprintf(msg, sizeof(msg), "%zu|%td|%jd|%zx|%d",
		(size_t)42, (ptrdiff_t)-7, (intmax_t)123456789012LL, (size_t)255, 9);
	TEST(23, "42|-7|123456789012|ff|9", ret);

	ret = snprintf(msg, sizeof(msg), "%zu", (size_t)SIZE_MAX);
	mu_check(msg[0] != '-');
	mu_check(strtoull(msg, NULL, 10) == (unsigned long long)SIZE_MAX);
}

MU_TEST(test_double_large_integer_values_are_exact) {
	int ret = snprintf(msg, sizeof(msg), "%.0f", 171798714241.0);
	TEST(12, "171798714241", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 123456789012345678.0);
	TEST(18, "123456789012345680", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", -123456789012345678.0);
	TEST(19, "-123456789012345680", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 9007199254740993.0);
	TEST(16, "9007199254740992", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 4503599627370497.0);
	TEST(16, "4503599627370497", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 1e22);
	TEST(23, "10000000000000000000000", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 1e23);
	TEST(23, "99999999999999991611392", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 18446744073709551616.0);
	TEST(20, "18446744073709551616", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 1180591620717411303424.0);
	TEST(22, "1180591620717411303424", ret);

	ret = snprintf(msg, sizeof(msg), "%f", 1099511627776.25);
	TEST(20, "1099511627776.250000", ret);
}

MU_TEST(test_double_fraction_digits_are_exact) {
	int ret;

	ret = snprintf(msg, sizeof(msg), "%.2f", 1.005);
	TEST(4, "1.00", ret);

	ret = snprintf(msg, sizeof(msg), "%.2f", 2.675);
	TEST(4, "2.67", ret);

	ret = snprintf(msg, sizeof(msg), "%.2f", 0.995);
	TEST(4, "0.99", ret);

	ret = snprintf(msg, sizeof(msg), "%.2f", 0.125);
	TEST(4, "0.12", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 0.5);
	TEST(1, "0", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 1.5);
	TEST(1, "2", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 2.5);
	TEST(1, "2", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 3.5);
	TEST(1, "4", ret);

	ret = snprintf(msg, sizeof(msg), "%.1f", 0.25);
	TEST(3, "0.2", ret);

	ret = snprintf(msg, sizeof(msg), "%.1f", 0.35);
	TEST(3, "0.3", ret);

	ret = snprintf(msg, sizeof(msg), "%.1f", 0.05);
	TEST(3, "0.1", ret);

	ret = snprintf(msg, sizeof(msg), "%.3f", 1.0005);
	TEST(5, "1.000", ret);

	ret = snprintf(msg, sizeof(msg), "%.6f", 674494634688.05493);
	TEST(19, "674494634688.054932", ret);

	ret = snprintf(msg, sizeof(msg), "%.2f", 34593980428896.605);
	TEST(17, "34593980428896.61", ret);

	ret = snprintf(msg, sizeof(msg), "%.20f", 0.1);
	TEST(22, "0.10000000000000000555", ret);

	ret = snprintf(msg, sizeof(msg), "%.6f", 0.9999996);
	TEST(8, "1.000000", ret);

	ret = snprintf(msg, sizeof(msg), "%.2f", 9.995);
	TEST(4, "9.99", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", 999999.5);
	TEST(7, "1000000", ret);

	ret = snprintf(msg, sizeof(msg), "%.2f", -0.001);
	TEST(5, "-0.00", ret);

	ret = snprintf(msg, sizeof(msg), "%.10f", 5e-11);
	TEST(12, "0.0000000001", ret);

	ret = snprintf(msg, sizeof(msg), "%.29f", 1e-30);
	TEST(31, "0.00000000000000000000000000000", ret);

	ret = snprintf(msg, sizeof(msg), "%.29f", 0.3);
	TEST(31, "0.29999999999999998889776975375", ret);
}

MU_TEST(test_double_sign_flags_of_zero_and_both_flags) {
	int ret = snprintf(msg, sizeof(msg), "%+f|% f", 0.0, 0.0);
	TEST(19, "+0.000000| 0.000000", ret);

	ret = snprintf(msg, sizeof(msg), "%+.0f|% .0f", 0.0, 0.0);
	TEST(5, "+0| 0", ret);

	// the plus flag overrides the space flag
	ret = snprintf(msg, sizeof(msg), "% +.1f|%+ .1f", 2.5, 2.5);
	TEST(9, "+2.5|+2.5", ret);

	ret = snprintf(msg, sizeof(msg), "%+ .1f", -2.5);
	TEST(4, "-2.5", ret);

	ret = snprintf(msg, sizeof(msg), "% +012.2f", 12.5);
	TEST(12, "+00000012.50", ret);
}

MU_TEST(test_double_e_and_g_digits_are_exact) {
	int ret;

	ret = snprintf(msg, sizeof(msg), "%.0e", 2.5);
	TEST(5, "2e+00", ret);

	ret = snprintf(msg, sizeof(msg), "%.0e", 0.5);
	TEST(5, "5e-01", ret);

	ret = snprintf(msg, sizeof(msg), "%.0e", 1.5);
	TEST(5, "2e+00", ret);

	ret = snprintf(msg, sizeof(msg), "%.0e", 0.25);
	TEST(5, "2e-01", ret);

	ret = snprintf(msg, sizeof(msg), "%.1e", 0.125);
	TEST(7, "1.2e-01", ret);

	ret = snprintf(msg, sizeof(msg), "%.1e", 0.375);
	TEST(7, "3.8e-01", ret);

	ret = snprintf(msg, sizeof(msg), "%.2e", 1.005);
	TEST(8, "1.00e+00", ret);

	ret = snprintf(msg, sizeof(msg), "%.3e", 9.9995);
	TEST(9, "9.999e+00", ret);

	ret = snprintf(msg, sizeof(msg), "%e", 1e23);
	TEST(12, "1.000000e+23", ret);

	ret = snprintf(msg, sizeof(msg), "%.20e", 0.1);
	TEST(26, "1.00000000000000005551e-01", ret);

	ret = snprintf(msg, sizeof(msg), "%e", 9.9999995);
	TEST(12, "9.999999e+00", ret);

	ret = snprintf(msg, sizeof(msg), "%.20e", 1e100);
	TEST(27, "1.00000000000000001590e+100", ret);

	ret = snprintf(msg, sizeof(msg), "%.20e", 5e-324);
	TEST(27, "4.94065645841246544177e-324", ret);

	ret = snprintf(msg, sizeof(msg), "%.1e", 9.96);
	TEST(7, "1.0e+01", ret);

	ret = snprintf(msg, sizeof(msg), "%e", 123456789012345678.0);
	TEST(12, "1.234568e+17", ret);

	ret = snprintf(msg, sizeof(msg), "%g", 123456.5);
	TEST(6, "123456", ret);

	ret = snprintf(msg, sizeof(msg), "%g", 123457.5);
	TEST(6, "123458", ret);

	ret = snprintf(msg, sizeof(msg), "%.3g", 999.9);
	TEST(5, "1e+03", ret);

	ret = snprintf(msg, sizeof(msg), "%.4g", 9.9996);
	TEST(2, "10", ret);

	ret = snprintf(msg, sizeof(msg), "%.0g", 2.5);
	TEST(1, "2", ret);

	ret = snprintf(msg, sizeof(msg), "%.1g", 0.95);
	TEST(3, "0.9", ret);

	ret = snprintf(msg, sizeof(msg), "%.1g", 0.25);
	TEST(3, "0.2", ret);

	ret = snprintf(msg, sizeof(msg), "%#.3g", -0.99965920105744954);
	TEST(5, "-1.00", ret);

	ret = snprintf(msg, sizeof(msg), "%g", 1e100);
	TEST(6, "1e+100", ret);

	ret = snprintf(msg, sizeof(msg), "%G", 1e-5);
	TEST(5, "1E-05", ret);

	ret = snprintf(msg, sizeof(msg), "%.29g", 0.1);
	TEST(31, "0.10000000000000000555111512313", ret);

	ret = snprintf(msg, sizeof(msg), "%g", 0.00012345678);
	TEST(11, "0.000123457", ret);

	ret = snprintf(msg, sizeof(msg), "%.10g", 1234567.8912345);
	TEST(11, "1234567.891", ret);

	ret = snprintf(msg, sizeof(msg), "%g", 99999.95);
	TEST(7, "99999.9", ret);

	ret = snprintf(msg, sizeof(msg), "%g", 999999.5);
	TEST(5, "1e+06", ret);

	ret = snprintf(msg, sizeof(msg), "%.2g", 0.000099999);
	TEST(6, "0.0001", ret);
}

MU_TEST(test_double_negative_zero) {
	int ret;

	ret = snprintf(msg, sizeof(msg), "%f", -0.0);
	TEST(9, "-0.000000", ret);

	ret = snprintf(msg, sizeof(msg), "%+.1f", -0.0);
	TEST(4, "-0.0", ret);

	ret = snprintf(msg, sizeof(msg), "%e", -0.0);
	TEST(13, "-0.000000e+00", ret);

	ret = snprintf(msg, sizeof(msg), "%g", -0.0);
	TEST(2, "-0", ret);

	ret = snprintf(msg, sizeof(msg), "%08.2f", -0.0);
	TEST(8, "-0000.00", ret);

	ret = snprintf(msg, sizeof(msg), "% .1e", -0.0);
	TEST(8, "-0.0e+00", ret);

	ret = snprintf(msg, sizeof(msg), "%+g", -0.0);
	TEST(2, "-0", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", -0.0);
	TEST(2, "-0", ret);

	ret = snprintf(msg, sizeof(msg), "%.0f", -0.4);
	TEST(2, "-0", ret);

	ret = snprintf(msg, sizeof(msg), "%-9.1f|", -0.0);
	TEST(10, "-0.0     |", ret);
}

MU_TEST(test_double_big_integral_part_is_exact) {
	char big[400];
	int ret = snprintf(big, sizeof(big), "%.0f", 1e100);
	mu_assert_int_eq(101, ret);
	mu_assert_string_eq(
		"1000000000000000015902891109759918046836080856394528138978132755"
		"7747838772170381060813469985856815104", big);

	ret = snprintf(big, sizeof(big), "%.0f", -DBL_MAX);
	mu_assert_int_eq(310, ret);
	mu_assert_string_eq("-"
		"1797693134862315708145274237317043567980705675258449965989174768"
		"0315726078002853876058955863276687817154045895351438246423432132"
		"6889464182768467546703537516986049910576551282076245490090389328"
		"9440758685084551339423045832369032229481658085593321233482747978"
		"26204144723168738177180919299881250404026184124858368", big);

	ret = snprintf(big, sizeof(big), "%.2f", DBL_MAX);
	mu_assert_int_eq(312, ret);
	mu_check(strncmp(big, "179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368", 309) == 0);
	mu_assert_string_eq(".00", big + 309);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

MU_TEST(test_prefix_is_not_skipped_when_buffer_is_null_or_full) {
	// the 0x, 0 prefixes and inf must be consumed even if they do not fit
	int ret = snprintf(NULL, 0, "%#x|%#o|%p", 255u, 8u, (void *)0x1234);
	mu_assert_int_eq(15, ret);

	ret = snprintf(msg, 2, "%#x", 255u);
	TEST(RETURNED(4, 1), "0", ret);

	ret = snprintf(msg, 3, "%p", (void *)0x1234);
	TEST(RETURNED(6, 2), "0x", ret);

	ret = snprintf(msg, 2, "%f", INFINITY);
	TEST(RETURNED(3, 1), "i", ret);

	ret = snprintf(NULL, 0, "%f", -INFINITY);
	mu_assert_int_eq(4, ret);

	// the sign of a zero padded number is written once, with or without buffer
	ret = snprintf(NULL, 0, "%012.3f", -1.5);
	mu_assert_int_eq(12, ret);

	ret = snprintf(NULL, 0, "%+012.2e", -1.5);
	mu_assert_int_eq(12, ret);

	ret = snprintf(msg, sizeof(msg), "%012.3f", -1.5);
	TEST(12, "-0000001.500", ret);
}

MU_TEST(test_truncated_output_returns_the_whole_length) {
	// the C library returns the length of the whole output, so a caller can
	// tell that the output was truncated and retry with a bigger buffer
	int ret = snprintf(msg, 4, "%s-%s", "aaaaaaaaaa", "bbbbbbbbbb");
	TEST(RETURNED(21, 3), "aaa", ret);

	ret = snprintf(msg, 1, "hello");
	TEST(RETURNED(5, 0), "", ret);

	ret = snprintf(msg, 8, "%.99s", "12345678901234567890");
	TEST(RETURNED(20, 7), "1234567", ret);

	// the conversions after the truncation are counted, and the whole format
	// is processed, also the padding of a wide field
	ret = snprintf(msg, 4, "%d%s", 12, "abc");
	TEST(RETURNED(5, 3), "12a", ret);

	ret = snprintf(msg, 4, "%20d", 7);
	TEST(RETURNED(20, 3), "   ", ret);

	// the same length as with a NULL buffer, and as with a big enough buffer
	ret = snprintf(msg, 4, "%s", "0123456789ABCDEF");
	TEST(RETURNED(16, 3), "012", ret);
	mu_assert_int_eq(16, snprintf(NULL, 0, "%s", "0123456789ABCDEF"));

	char big[64];
	mu_assert_int_eq(16, snprintf(big, sizeof(big), "%s", "0123456789ABCDEF"));
	mu_assert_string_eq("0123456789ABCDEF", big);
}

MU_TEST(test_truncated_output_does_not_write_past_the_buffer) {
	// a guard after the buffer must stay untouched, also for a wide field
	// whose padding does not fit at all
	struct {
		char text[4];
		unsigned char guard[8];
	} b;
	memset(&b, 0x5a, sizeof(b));
	mu_assert_int_eq(RETURNED(20, 3), snprintf(b.text, sizeof(b.text), "%20d", 7));
	mu_assert_string_eq("   ", b.text); /* the padding fits, the digit does not */
	for (size_t i = 0; i < sizeof(b.guard); i++) {
		mu_check(b.guard[i] == 0x5a);
	}

	// the field width is counted even when not a single byte of it fits
	mu_assert_int_eq(RETURNED(11, 0), snprintf(b.text, 1, "%10d|", 7));
	mu_assert_string_eq("", b.text);
}

MU_TEST(test_counter_is_the_whole_length_when_truncated) {
	// %n is reached and stores the length of the output before it, in all modes
	int counter = -1;
	int ret = snprintf(msg, 8, "ab%n", &counter);
	TEST(2, "ab", ret);
	mu_assert_int_eq(2, counter);

	// a truncation after %n does not change what it stored
	counter = -1;
	ret = snprintf(msg, 4, "a%n-bcdef", &counter);
	TEST(RETURNED(7, 3), "a-b", ret);
	mu_assert_int_eq(1, counter);
}

MU_TEST(test_counter_reached_after_a_truncation) {
	// What %n stores when the buffer was already full when it is reached.
	// The legacy mode alone stops the output at the truncation, so %n is never
	// reached. The strict mode keeps validating the format and reaches it, and
	// it stores the number of characters written, like the whole output without.
	int counter = -1;
	int ret = snprintf(msg, 4, "abcd%n", &counter);
	TEST(RETURNED(4, 3), "abc", ret);
#if defined(SNPRINTF_LEGACY_LENGTH) && !defined(SNPRINTF_STRICT)
	mu_check(counter == -1); /* not stored */
#elif defined(SNPRINTF_LEGACY_LENGTH)
	mu_assert_int_eq(3, counter);
#else
	mu_assert_int_eq(4, counter);
#endif

	// the same for a conversion whose text does not fit at all
	counter = -1;
	ret = snprintf(msg, 2, "%d%n", 2020, &counter);
	TEST(RETURNED(4, 1), "2", ret);
#if defined(SNPRINTF_LEGACY_LENGTH) && !defined(SNPRINTF_STRICT)
	mu_check(counter == -1); /* not stored */
#elif defined(SNPRINTF_LEGACY_LENGTH)
	mu_assert_int_eq(1, counter);
#else
	mu_assert_int_eq(4, counter);
#endif
}

MU_TEST(test_long_output_is_reported_as_int_max) {
	// a length that does not fit into an int must not turn into an error
	int ret = snprintf(NULL, 0, "%*d", INT_MAX, 7);
	mu_assert_int_eq(INT_MAX, ret);

	ret = snprintf(msg, 4, "%*d", INT_MAX, 7);
	mu_assert_int_eq(RETURNED(INT_MAX, 3), ret);
	mu_assert_string_eq("   ", msg);
}

MU_TEST(test_counters) {
	int counter1 = 0, counter2 = 0;
	int ret = snprintf(msg, sizeof(msg), "%s%n %s%n%c",
		"Hello", &counter1, "World", &counter2, '!');
	TEST(12, "Hello World!", ret);
	mu_assert_int_eq(5, counter1);
	mu_assert_int_eq(11, counter2);
}

MU_TEST(test_counter_length_modifiers) {
	struct {
		signed char value;
		unsigned char canary[4];
	} hh = {0, {0xA5, 0xA5, 0xA5, 0xA5}};
	struct {
		short value;
		unsigned char canary[4];
	} h = {0, {0xA5, 0xA5, 0xA5, 0xA5}};
	long l = -1;
	long long ll = -1;
	int ret = snprintf(msg, sizeof(msg), "abc%hhn", &hh.value);
	TEST(3, "abc", ret);
	mu_assert_int_eq(3, hh.value);
	for (size_t i = 0; i < sizeof(hh.canary); i++) {
		mu_assert_int_eq(0xA5, hh.canary[i]);
	}

	ret = snprintf(msg, sizeof(msg), "abc%hn", &h.value);
	TEST(3, "abc", ret);
	mu_assert_int_eq(3, h.value);
	for (size_t i = 0; i < sizeof(h.canary); i++) {
		mu_assert_int_eq(0xA5, h.canary[i]);
	}

	ret = snprintf(msg, sizeof(msg), "abc%ln", &l);
	TEST(3, "abc", ret);
	mu_assert_int_eq(3, l);

	ret = snprintf(msg, sizeof(msg), "abc%lln", &ll);
	TEST(3, "abc", ret);
	mu_assert_int_eq(3, ll);
}


MU_TEST_SUITE(test_suite) {
	MU_RUN_TEST(test_buffer_null);

	MU_RUN_TEST(test_buffer_length_0);
	MU_RUN_TEST(test_buffer_length_1);
	MU_RUN_TEST(test_buffer_length_2);
	MU_RUN_TEST(test_buffer_length_3);

	MU_RUN_TEST(test_wrong_format_no_type);
	MU_RUN_TEST(test_wrong_format_unsupported_type);
	MU_RUN_TEST(test_plus_flag_and_left_align);
#ifndef SNPRINTF_STRICT
	MU_RUN_TEST(test_malformed_format_standard_like);
#else
	MU_RUN_TEST(test_strict_mode_rejects_malformed_specifier);
#endif

	MU_RUN_TEST(test_char_dec);
	MU_RUN_TEST(test_char_dec_min_and_max);
	MU_RUN_TEST(test_char_dec_negative);

	MU_RUN_TEST(test_short_dec);
	MU_RUN_TEST(test_short_dec_min_and_max);
	MU_RUN_TEST(test_short_dec_negative);

	MU_RUN_TEST(test_int_dec);
	MU_RUN_TEST(test_int_dec_min_and_max);
	MU_RUN_TEST(test_int_dec_negative);
	MU_RUN_TEST(test_int_dec_width_10);
	MU_RUN_TEST(test_int_dec_width_31_and_0_padded);
	MU_RUN_TEST(test_int_dec_width_31_and_align_left);
	MU_RUN_TEST(test_int_dec_width_2);
	MU_RUN_TEST(test_int_dec_width_20_precision_10);
	MU_RUN_TEST(test_int_dec_precision_0);
	MU_RUN_TEST(test_int_dec_width_as_parameter);
	MU_RUN_TEST(test_int_dynamic_precision);
	MU_RUN_TEST(test_int_dec_random);
	MU_RUN_TEST(test_int_i_length_modifiers);

	MU_RUN_TEST(test_int_hex);
	MU_RUN_TEST(test_int_hex_uppercase);
	MU_RUN_TEST(test_int_hex_negative);
	MU_RUN_TEST(test_int_hex_precision_0);
	MU_RUN_TEST(test_octal_alternative_form);
	MU_RUN_TEST(test_unsigned_long_and_octal_lengths);

	MU_RUN_TEST(test_long_dec);
	MU_RUN_TEST(test_long_hex);
	MU_RUN_TEST(test_long_hex_alternative);
	MU_RUN_TEST(test_long_hex_width_as_type);

	MU_RUN_TEST(test_long_long_dec);
	MU_RUN_TEST(test_long_long_dec_min);
	MU_RUN_TEST(test_long_long_dec_max);
	MU_RUN_TEST(test_long_long_unsigned_max);
	MU_RUN_TEST(test_long_long_unsigned_sign_flags);
	MU_RUN_TEST(test_long_long_zero_pad_negative);
	MU_RUN_TEST(test_long_long_hex_zero_pad_alternative);
	MU_RUN_TEST(test_long_long_hex);
	MU_RUN_TEST(test_long_long_hex_alternative);
	MU_RUN_TEST(test_long_long_hex_width_as_type);
	MU_RUN_TEST(test_long_long_hex_max);

	MU_RUN_TEST(test_double_f);
	MU_RUN_TEST(test_double_f_precision_0);
	MU_RUN_TEST(test_double_f_precision_2_3);
	MU_RUN_TEST(test_double_e);
	MU_RUN_TEST(test_double_e_zero);
	MU_RUN_TEST(test_double_e_precision_0);
	MU_RUN_TEST(test_double_e_precision_2_3);
	MU_RUN_TEST(test_double_g);
	MU_RUN_TEST(test_double_g_precision_0);
	MU_RUN_TEST(test_double_g_precision_2_7);
	MU_RUN_TEST(test_double_g_alternate_form_and_significant_precision);
	MU_RUN_TEST(test_double_negative_and_sign_flags);
	MU_RUN_TEST(test_double_g_boundaries_and_trim);
	MU_RUN_TEST(test_double_e_rounds_mantissa);
	MU_RUN_TEST(test_double_extreme_exponents_and_precision);
	MU_RUN_TEST(test_double_finite_limits);
	MU_RUN_TEST(test_double_large_fixed_truncates_safely);
	MU_RUN_TEST(test_special_float_values);
	MU_RUN_TEST(test_extreme_format_width_and_precision);
	MU_RUN_TEST(test_double_null_buffer_and_truncation);
	MU_RUN_TEST(test_double_fraction_buffer_boundary);
	MU_RUN_TEST(test_double_width_and_zero_padding);
	MU_RUN_TEST(test_double_dynamic_width_and_precision);
	MU_RUN_TEST(test_double_alternate_form_zero_precision_width);

	MU_RUN_TEST(test_string_null_pointer);
	MU_RUN_TEST(test_string);
	MU_RUN_TEST(test_string_empty);
	MU_RUN_TEST(test_string_width_20);
	MU_RUN_TEST(test_string_width_20_and_align_left);
	MU_RUN_TEST(test_string_width_20_precision_2);
	MU_RUN_TEST(test_string_width_20_precision_20);
	MU_RUN_TEST(test_string_with_less_than_input);
	MU_RUN_TEST(test_string_with_less_than_input_precision_equal_width);
	MU_RUN_TEST(test_string_width_as_parameter);
	MU_RUN_TEST(test_string_precision_as_parameter);
	MU_RUN_TEST(test_string_precision_nonterminated_span);
	MU_RUN_TEST(test_string_width_and_precision_as_parameter);
	MU_RUN_TEST(test_string_width_as_parameter_negative);
	MU_RUN_TEST(test_string_too_long);

	MU_RUN_TEST(test_strings);
	MU_RUN_TEST(test_chars);
	MU_RUN_TEST(test_char_width_and_alignment);

	MU_RUN_TEST(test_pointer_null);
	MU_RUN_TEST(test_pointer);
	MU_RUN_TEST(test_pointer_width);

	MU_RUN_TEST(test_percent);
	MU_RUN_TEST(test_counters);
	MU_RUN_TEST(test_counter_length_modifiers);

	MU_RUN_TEST(test_unsigned_long_long_max);
	MU_RUN_TEST(test_unsigned_long_max);
	MU_RUN_TEST(test_unsigned_ignores_plus_and_space_flags);
	MU_RUN_TEST(test_pointer_dynamic_width);
	MU_RUN_TEST(test_left_align_ignores_zero_flag);
	MU_RUN_TEST(test_lone_dot_is_precision_zero);
	MU_RUN_TEST(test_hex_alternative_form_of_zero);
	MU_RUN_TEST(test_zero_flag_ignored_with_integer_precision);
#ifndef SNPRINTF_STRICT
	MU_RUN_TEST(test_zero_flag_on_string_pads_with_blanks);
#endif
	MU_RUN_TEST(test_size_ptrdiff_and_intmax_lengths);
	MU_RUN_TEST(test_double_large_integer_values_are_exact);
	MU_RUN_TEST(test_double_fraction_digits_are_exact);
	MU_RUN_TEST(test_double_sign_flags_of_zero_and_both_flags);
	MU_RUN_TEST(test_double_e_and_g_digits_are_exact);
	MU_RUN_TEST(test_double_negative_zero);
	MU_RUN_TEST(test_double_big_integral_part_is_exact);
	MU_RUN_TEST(test_prefix_is_not_skipped_when_buffer_is_null_or_full);
	MU_RUN_TEST(test_truncated_output_returns_the_whole_length);
	MU_RUN_TEST(test_truncated_output_does_not_write_past_the_buffer);
	MU_RUN_TEST(test_counter_is_the_whole_length_when_truncated);
	MU_RUN_TEST(test_counter_reached_after_a_truncation);
	MU_RUN_TEST(test_long_output_is_reported_as_int_max);
}


int tests_snprintf(void) {
	MU_RUN_SUITE(test_suite);
	MU_REPORT();
	return MU_EXIT_CODE;
}


#ifdef __clang__
#pragma clang diagnostic pop
#endif
