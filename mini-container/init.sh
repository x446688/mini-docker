#!/bin/bash
CONTAINER_DIR=$1
TEST_FILE=$2
test -d $CONTAINER_DIR || mkdir $CONTAINER_DIR
curl -o minrootfs.tar.xz https://mirror.rosalab.ru/rosa/rosa2021.1/iso/ROSA.FRESH.12/rootfs/rootfs-minimal-rosa2021.1_x86_64_2022-11-02.tar.xz
tar -xvf minrootfs.tar.xz -C $CONTAINER_DIR
cp $TEST_FILE $CONTAINER_DIR/usr/bin
rm *.xz
