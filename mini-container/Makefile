.PHONY: all clean run-current
VERSION ?= $(shell cat .version)
CC = gcc
TARGET = test
CFLAGS ?= -I. -I../mini-logger/
LDFLAGS ?= -lcap
all: tests/test.c create.c create.h
	test -d build/$(VERSION) || mkdir -p build/$(VERSION)
	$(CC) tests/test.c create.c ../mini-logger/logger.c $(CFLAGS) -o build/$(VERSION)/$(TARGET) $(LDFLAGS)
debug:
	test -d build/debug || mkdir -p build/debug
	$(CC) tests/test.c create.c ../mini-logger/logger.c $(CFLAGS) -g -o build/debug/$(TARGET)-debug $(LDFLAGS)
clean:
	rm -rf build
run-current: all
	test -f build/$(VERSION)/$(TARGET) && ./build/$(VERSION)/$(TARGET)
run-debug: debug
	test -f build/debug/$(TARGET)-debug && ./build/debug/$(TARGET)-debug
