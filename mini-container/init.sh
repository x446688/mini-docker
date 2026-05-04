#!/bin/bash
LIBDIR=lib
LIBDIR64=lib64
CONTAINER_DIR=$1
TEST_FILE=$2
if !(ls ${CONTAINER_DIR}); then
    mkdir -p ${CONTAINER_DIR}/usr/bin ${CONTAINER_DIR}/${LIBDIR} ${CONTAINER_DIR}/${LIBDIR64} ${CONTAINER_DIR}/var/log/
    cp /usr/bin/busybox ${CONTAINER_DIR}/usr/bin
    cp ${TEST_FILE} ${CONTAINER_DIR}/usr/bin
    ldd /usr/bin/busybox | awk '/ => / { print $3 }' > tmp.txt
    FILE="tmp.txt"
    while read -r line; do
        cp "$line" ${CONTAINER_DIR}/${LIBDIR}/.
        cp "$line" ${CONTAINER_DIR}/${LIBDIR64}/.
    done <$FILE
    rm tmp.txt
    cp /${LIBDIR64}/ld-linux-*.so.* ${CONTAINER_DIR}/${LIBDIR64}
fi