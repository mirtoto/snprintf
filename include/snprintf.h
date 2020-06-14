// Copyright (C) 2019 Miroslaw Toton, mirtoto@gmail.com
#ifndef SNPRINTF_H_
#define SNPRINTF_H_


#include <stdarg.h>
#include <stddef.h>


int vsnprintf(char *string, size_t length, const char *format, va_list args) __attribute__((format(printf, 3, 0)));
int snprintf(char *string, size_t length, const char *format, ...) __attribute__((format(printf, 3, 4)));


#endif  // SNPRINTF_H_
