# firmware/tests —— core/ 的 PC 端单元测试
#
# 用法：
#   make          # 编译并运行
#   make clean
#
# 这些测试只编译 firmware/core/，完全不依赖 STM32 头文件，
# 因此可以在任何有 gcc 的机器上跑，也是验收标准 A7「测试证据」的原始材料。

CC     ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -I../core

SRCS := $(wildcard ../core/*.c) test_main.c test_gate_counter.c test_queue_grade.c test_protocol.c
BIN  := run_tests

.PHONY: all test clean
all: $(BIN) test

$(BIN): $(SRCS) $(wildcard ../core/*.h) test_util.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test: $(BIN)
	./$(BIN)

clean:
	-rm -f $(BIN) $(BIN).exe
