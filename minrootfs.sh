#!/bin/bash
CONTAINER_DIR=$1
test -d $CONTAINER_DIR || mkdir $CONTAINER_DIR
if [ -z "$( ls -A $CONTAINER_DIR )" ]; then
    test -f minrootfs.tar.xz || curl -o minrootfs.tar.xz https://file-store.rosa.ru/api/v1/file_stores/2de7c1fc0a370043b883337ff57d96f16c40de92
    tar -xvf minrootfs.tar.xz -C $CONTAINER_DIR
    rm *.xz
    cp /usr/bin/mini-test $CONTAINER_DIR/bin
fi
