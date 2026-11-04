CC     ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

uzkbd: src/main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f uzkbd

.PHONY: clean
