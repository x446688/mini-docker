#!/bin/bash
CONTAINER_DIR=$1
TEST_FILE=$2
test -d $CONTAINER_DIR || mkdir $CONTAINER_DIR
if [ -z "$( ls -A $CONTAINER_DIR )" ]; then
    test -f minrootfs.tar.xz || curl -o minrootfs.tar.xz https://file-store.rosa.ru/api/v1/file_stores/2de7c1fc0a370043b883337ff57d96f16c40de92
    tar -xvf minrootfs.tar.xz -C $CONTAINER_DIR
    rm *.xz
fi
cp $TEST_FILE $CONTAINER_DIR/usr/bin
