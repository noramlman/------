# ============================================================
#  图片调试工具  Makefile  (MinGW / MSYS2 / gcc)
#
#  用法（在本目录执行）：
#      mingw32-make          编译
#      mingw32-make run      编译 + 跑默认图片
#      mingw32-make clean    清理
# ============================================================

CC      ?= gcc
CFLAGS  := -std=c99 -Wall -Wextra -O2 -Isrc
SRCDIR  := src

SRCS    := $(SRCDIR)/main.c $(SRCDIR)/bmp_io.c $(SRCDIR)/user_proc.c
TARGET  := bmp_tool

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(wildcard $(SRCDIR)/*.h)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) images/0.bmp out.bmp
	@echo "用看图软件打开 out.bmp"

clean:
	rm -f $(TARGET) *.o out*.bmp
