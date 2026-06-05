#!/bin/bash
CONTAINER_DIR=$1
test -d $CONTAINER_DIR || mkdir $CONTAINER_DIR
if [ -z "$( ls -A $CONTAINER_DIR )" ]; then
    test -f minrootfs.tar.xz || curl -o minrootfs.tar.xz https://file-store.rosa.ru/api/v1/file_stores/cb8104176b37ca7a73d93ff77add8fe9a4b190a9
    tar -xvf minrootfs.tar.xz -C $CONTAINER_DIR
    rm *.xz
    cp /usr/bin/mini-test $CONTAINER_DIR/bin
fi
