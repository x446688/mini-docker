.PHONY: all clean run-current
VERSION ?= $(shell cat .version)
CC = gcc
TARGET = mini-docker
CFLAGS ?= -I. -Imini-logger/
LDFLAGS ?= -lcap
all: main.c mini-container/create.c mini-container/create.h
	@if [ ! -f /usr/bin/mini-docker-init ]; then \
		cp mini-container/init.sh /usr/bin; \
		mv /usr/bin/init.sh /usr/bin/mini-docker-init; \
	fi
	test -d build/$(VERSION) || mkdir -p build/$(VERSION)
	gcc -o mini-test tests/mini-test.c
	mv mini-test /usr/bin/
	$(CC) main.c mini-container/create.c mini-logger/logger.c $(CFLAGS) -o build/$(VERSION)/$(TARGET) $(LDFLAGS)
install:
	test -d /run/mini-docker || mkdir /run/mini-docker
	touch /var/log/mini-docker.log
	chmod -R 775 /var/log/mini-docker.log
	getent group daemoner || addgroup --system daemoner
	id daemoner || adduser --system --ingroup daemoner daemoner
	chown -R root:daemoner /var/log/mini-docker.log
	chown -R daemoner:daemoner /run/mini-docker
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