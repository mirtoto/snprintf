// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com
#include "snprintf.h"

#include "minunit.h"


static char msg[32] = {0, };


#define TEST(ret_expected, msg_expected, ret) { 		\
	mu_assert_int_eq((ret_expected), ret); 				\
	if ((ret) >= 0) { 									\
		mu_check(msg[(ret)] == '\0'); 					\
		mu_assert_string_eq((msg_expected), msg); 		\
	} 													\
}


#if __GNUC__ >= 7
#pragma GCC diagnostic push
// We're testing that truncation works properly, so temporarily disable the warning.
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

MU_TEST(test_buffer_length_0) {
	int ret = snprintf(msg, 0, "%d", 123);
	TEST(-1, NULL, ret);
}

MU_TEST(test_buffer_length_1) {
	int ret = snprintf(msg, 1, "%d", 123);
	TEST(0, "",	ret);
}

MU_TEST(test_buffer_length_2) {
	int ret = snprintf(msg, 2, "%d", 123);
	TEST(1, "1", ret);
}

#if __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif

MU_TEST(test_char_dec) {
	int ret = snprintf(msg, sizeof(msg), "%hhd% hhd %hhu",
		(char)123, (char)123, (char)123);
	TEST(11, "123 123 123", ret);
}

MU_TEST(test_char_dec_negative) {
	int ret = snprintf(msg, sizeof(msg), "%hhd% hhd %hhu",
		(char)-123, (char)-123, (char)-123);
	TEST(12, "-123-123 133", ret);
}

MU_TEST(test_short_dec) {
	int ret = snprintf(msg, sizeof(msg), "%hd% hd %hu",
		(short)1230, (short)1230, (short)1230);
	TEST(14, "1230 1230 1230", ret);
}

MU_TEST(test_short_dec_negative) {
	int ret = snprintf(msg, sizeof(msg), "%hd% hd %hu",
		(short)-1230, (short)-1230, (short)-1230);
	TEST(16, "-1230-1230 64306", ret);
}

MU_TEST(test_int_dec) {
	int ret = snprintf(msg, sizeof(msg), "%d% d %u", 123, 123, 123);
	TEST(11, "123 123 123", ret);
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

MU_TEST(test_int_dec_width_31_and_align_left) {
	int ret = snprintf(msg, sizeof(msg), "%-31d", 123);
	TEST(31, "123                            ", ret);
}

MU_TEST(test_int_dec_width_2) {
	int ret = snprintf(msg, sizeof(msg), "%2d", 123);
	TEST(3, "123", ret);
}

MU_TEST(test_int_dec_width_as_parameter) {
	int ret = snprintf(msg, sizeof(msg), "%*d", 5, 123);
	TEST(5, "  123", ret);
}

MU_TEST(test_int_hex) {
	int ret = snprintf(msg, sizeof(msg), "%x %#x", 123, 123);
	TEST(7, "7b 0x7b", ret);
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

MU_TEST(test_long_long_hex) {
	int ret = snprintf(msg, sizeof(msg), "%llx %llX", 123000000000ll, 123000000000ll);
	TEST(21, "1ca35f0e00 1CA35F0E00", ret);
}

MU_TEST(test_long_long_hex_alternative) {
	int ret = snprintf(msg, sizeof(msg), "%#llx %#llX", 123000000000ll, 123000000000ll);
	TEST(25, "0x1ca35f0e00 0X1CA35F0E00", ret);
}

MU_TEST(test_long_long_hex_width_as_type) {
	const char expected[] = "0000000000000000000000000000000000000000000000000000001ca35f0e00";
	long long x = 123000000000ll;
	int ret = snprintf(msg, sizeof(msg), "%0*llx", (int)(sizeof(x) * 2), x);
	TEST(sizeof(x) * 2, expected + strlen(expected) - sizeof(x) * 2, ret);
}

MU_TEST(test_double_f) {
	int ret = snprintf(msg, sizeof(msg), "%f %F",
		123.0, 123.0 + 1.0 / 3);
	TEST(21, "123.000000 123.333333", ret);
}

MU_TEST(test_double_f_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%.0f %.0F",
		123.0, 123.0 + 1.0 / 3);
	TEST(7, "123 123", ret);
}

MU_TEST(test_double_f_precision_2_3) {
	int ret = snprintf(msg, sizeof(msg), "%2.3f %2.3F",
		123.0, 123.0 + 1.0 / 3);
	TEST(15, "123.000 123.333", ret);
}

MU_TEST(test_double_e) {
	int ret = snprintf(msg, sizeof(msg), "%e %E",
		123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(27, "1.233333e+002 1.233333E+002", ret);
}

MU_TEST(test_double_e_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%.0e %.0E",
		123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(13, "1e+002 1E+002", ret);
}

MU_TEST(test_double_e_precision_2_3) {
	int ret = snprintf(msg, sizeof(msg), "%2.3e %2.3E",
		123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(21, "1.233e+002 1.233E+002", ret);
}

MU_TEST(test_double_g) {
	int ret = snprintf(msg, sizeof(msg), "%g %G",
		123.0 + 1.0 / 3, 123.0 + 1.0 / 3);
	TEST(21, "123.333333 123.333333", ret);
}

MU_TEST(test_double_g_precision_0) {
	int ret = snprintf(msg, sizeof(msg), "%.0g %.0G",
		1.0 / 123000000.0, 1.0 / 123000000.0);
	TEST(11, "8e-09 8E-09", ret);
}

MU_TEST(test_double_g_precision_2_7) {
	int ret = snprintf(msg, sizeof(msg), "%2.7g %2.7G",
		1.0 / 123000000.0, 1.0 / 123000000.0);
	TEST(27, "8.1300813e-09 8.1300813E-09", ret);
}

MU_TEST(test_string) {
	int ret = snprintf(msg, sizeof(msg), "%s", "Hello");
	TEST(5, "Hello", ret);
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

MU_TEST(test_string_too_long) {
	const char *str = "This is very long message and it is much longer than buffer!";
	int ret = snprintf(msg, sizeof(msg), "%s", str);
	TEST(sizeof(msg) - 1, "This is very long message and i", ret);
}

MU_TEST(test_strings) {
	int ret = snprintf(msg, sizeof(msg), "%s %s%c", "Hello", "World", '!');
	TEST(12, "Hello World!", ret);
}

MU_TEST(test_chars) {
	int ret = snprintf(msg, sizeof(msg), "%c%c%c%c%c", 'H', 'e', 'l', 'l', 'o');
	TEST(5, "Hello", ret);
}

MU_TEST(test_percent) {
	const char *str = "%%%%% Hello World! %%%%%";
	int ret = snprintf(msg, sizeof(msg), "%%%%%%%%%% Hello World! %%%%%%%%%%");
	TEST((int)strlen(str), str, ret);
}

MU_TEST(test_counters) {
	int counter1 = 0, counter2 = 0;
	int ret = snprintf(msg, sizeof(msg), "%s%n %s%n%c",
		"Hello", &counter1, "World", &counter2, '!');
	TEST(12, "Hello World!", ret);
	mu_assert_int_eq(5, counter1);
	mu_assert_int_eq(11, counter2);
}


MU_TEST_SUITE(test_suite) {
	MU_RUN_TEST(test_buffer_length_0);
	MU_RUN_TEST(test_buffer_length_1);
	MU_RUN_TEST(test_buffer_length_2);

	MU_RUN_TEST(test_char_dec);
	MU_RUN_TEST(test_char_dec_negative);

	MU_RUN_TEST(test_short_dec);
	MU_RUN_TEST(test_short_dec_negative);

	MU_RUN_TEST(test_int_dec);
	MU_RUN_TEST(test_int_dec_negative);
	MU_RUN_TEST(test_int_dec_width_10);
	MU_RUN_TEST(test_int_dec_width_31_and_0_padded);
	MU_RUN_TEST(test_int_dec_width_31_and_align_left);
	MU_RUN_TEST(test_int_dec_width_2);
	MU_RUN_TEST(test_int_dec_width_as_parameter);

	MU_RUN_TEST(test_int_hex);
	MU_RUN_TEST(test_int_hex_uppercase);
	MU_RUN_TEST(test_int_hex_negative);

	MU_RUN_TEST(test_long_dec);
	MU_RUN_TEST(test_long_hex);
	MU_RUN_TEST(test_long_hex_alternative);
	MU_RUN_TEST(test_long_hex_width_as_type);

	MU_RUN_TEST(test_long_long_dec);
	MU_RUN_TEST(test_long_long_hex);
	MU_RUN_TEST(test_long_long_hex_alternative);
	MU_RUN_TEST(test_long_long_hex_width_as_type);

	MU_RUN_TEST(test_double_f);
	MU_RUN_TEST(test_double_f_precision_0);
	MU_RUN_TEST(test_double_f_precision_2_3);
	MU_RUN_TEST(test_double_e);
	MU_RUN_TEST(test_double_e_precision_0);
	MU_RUN_TEST(test_double_e_precision_2_3);
	MU_RUN_TEST(test_double_g);
	MU_RUN_TEST(test_double_g_precision_0);
	MU_RUN_TEST(test_double_g_precision_2_7);

	MU_RUN_TEST(test_string);
	MU_RUN_TEST(test_string_width_20);
	MU_RUN_TEST(test_string_width_20_and_align_left);
	MU_RUN_TEST(test_string_width_20_precision_2);
	MU_RUN_TEST(test_string_width_20_precision_20);
	MU_RUN_TEST(test_string_too_long);

	MU_RUN_TEST(test_strings);
	MU_RUN_TEST(test_chars);

	MU_RUN_TEST(test_percent);
	MU_RUN_TEST(test_counters);
}


int tests_snprintf(void) {
	MU_RUN_SUITE(test_suite);
	MU_REPORT();
	return MU_EXIT_CODE;
}
