CC = gcc
CFLAGS = -Wall -Wextra
TARGET = aesdsocket
OBJFILES = aesdsocket.o

ifdef CROSS_COMPILE
    CC = $(CROSS_COMPILE)gcc
endif

.PHONY: all clean default

default: $(TARGET)
all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJFILES)
