/** @file test_main.c —— 测试入口 */
#include "test_util.h"

int g_tests = 0;
int g_fails = 0;

int main(void)
{
    printf("[gate_counter]\n");
    suite_gate_counter();
    printf("[queue_grade]\n");
    suite_queue_grade();
    printf("[protocol]\n");
    suite_protocol();

    printf("\n%d checks, %d failures\n", g_tests, g_fails);
    return (g_fails == 0) ? 0 : 1;
}
