CC = gcc
CFLAGS = -Wall -Wextra
LFLAGS = -lpthread
TARGET = aesdsocket
OBJFILES = aesdsocket.o

ifdef CROSS_COMPILE
    CC = $(CROSS_COMPILE)gcc
endif

.PHONY: all clean default

default: $(TARGET)
all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LFLAGS)

clean:
	rm -f $(TARGET) $(OBJFILES)
