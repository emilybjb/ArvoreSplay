CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -pthread
LDFLAGS = -lm

SRCS    = main.c cache.c splay_cache.c metrics.c
OBJS    = $(SRCS:.c=.o)
TARGET  = page_cache

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

tsan: $(SRCS)
	$(CC) -fsanitize=thread -g -o $(TARGET)_tsan $^ $(LDFLAGS)

asan: $(SRCS)
	$(CC) -fsanitize=address -g -o $(TARGET)_asan $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET)_tsan $(TARGET)_asan *.bin

.PHONY: all run tsan asan clean