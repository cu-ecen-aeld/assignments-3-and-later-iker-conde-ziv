CC ?= gcc
CFLAGS ?= -Wall -Wextra
LDFLAGS ?= -lpthread
TARGET ?= aesdsocket
OBJFILES = aesdsocket.o

ifdef CROSS_COMPILE
    CC ?= $(CROSS_COMPILE)gcc
endif

.PHONY: all clean default

default: $(TARGET)
all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJFILES)
