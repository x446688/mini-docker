.PHONY: all clean run-current
VERSION ?= $(shell cat .version)
CC = gcc
TARGET = mini-docker
CFLAGS ?= -I. -Imini-logger/ -Imini-container/
LDFLAGS ?= -lcap
all: main.c mini-container/create.c mini-container/create.h
	test -d build/$(VERSION) || mkdir -p build/$(VERSION)
	$(CC) tests/mini-test.c mini-logger/logger.c $(CFLAGS) -o mini-test $(LDFLAGS)
	mv mini-test /usr/bin/
	$(CC) main.c mini-container/create.c mini-logger/logger.c $(CFLAGS) -o build/$(VERSION)/$(TARGET) $(LDFLAGS)
install:
	test -d /run/mini-docker || mkdir /run/mini-docker
	touch /var/log/mini-docker.log
	chmod -R 775 /var/log/mini-docker.log
	chmod 755 /run/mini-docker
	echo "installed"
	cp build/$(VERSION)/$(TARGET) /usr/bin/
	cp mini-container.service /etc/systemd/system/
debug:
	test -d build/debug || mkdir -p build/debug
	$(CC) main.c mini-container/create.c mini-logger/logger.c $(CFLAGS) -g -o build/debug/$(TARGET)-debug $(LDFLAGS)
clean:
	rm -rf build
test: all
	./build/$(VERSION)/$(TARGET) -m "/tmp/mini-docker"; cat /var/log/mini-docker.log ; cat /tmp/mini-docker/var/log/mini-docker.log
