/** @file test_util.h  —— 极简断言宏（不引入第三方测试框架） */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

extern int g_tests;
extern int g_fails;

#define CHECK(cond)                                                      \
    do {                                                                 \
        g_tests++;                                                       \
        if (!(cond)) {                                                   \
            g_fails++;                                                   \
            printf("  FAIL %s:%d   %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                \
    } while (0)

#define CHECK_EQ(a, b)                                                   \
    do {                                                                 \
        g_tests++;                                                       \
        long _va = (long)(a);                                            \
        long _vb = (long)(b);                                            \
        if (_va != _vb) {                                                \
            g_fails++;                                                   \
            printf("  FAIL %s:%d   %s == %s  (got %ld, want %ld)\n",     \
                   __FILE__, __LINE__, #a, #b, _va, _vb);                \
        }                                                                \
    } while (0)

void suite_gate_counter(void);
void suite_queue_grade(void);
void suite_protocol(void);

#endif /* TEST_UTIL_H */
