CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
TARGET = fat32_test

SRCDIR = src
OBJDIR = obj

# 自動抓 src/*.c 所有檔案
SRCS = $(wildcard $(SRCDIR)/*.c)
# 將 src/*.c 轉成 obj/*.o
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# 產生 obj/xxx.o
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
