CFLAGS := -g -O0 -std=c99 -D_XOPEN_SOURCE=700 -Wall

all: target tracer

clean:
	rm -f target tracer

target: target.c
	$(CC) $(CFLAGS) $< -o $@

tracer: tracer.c
	$(CC) $(CFLAGS) $< -o $@

.PHONY: all clean
