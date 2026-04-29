.PHONY: all clean run-current
VERSION ?= $(shell cat .version)
CC = gcc
TARGET = mini-docker
CFLAGS ?= -I. -I mini-logger/
LDFLAGS ?= -lcap
all: main.c mini-container/create.c mini-container/create.h
	test -d build/$(VERSION) || mkdir -p build/$(VERSION)
	$(CC) main.c mini-container/create.c mini-logger/logger.c $(CFLAGS) -o build/$(VERSION)/$(TARGET) $(LDFLAGS)
debug:
	test -d build/debug || mkdir -p build/debug
	$(CC) main.c mini-container/create.c mini-logger/logger.c $(CFLAGS) -g -o build/debug/$(TARGET)-debug $(LDFLAGS)
clean:
	rm -rf build
test:
	./build/$(VERSION)/$(TARGET) -m "/tmp/mini-docker"; cat /var/log/mini-docker.log ; cat /tmp/mini-docker/mini-docker.log