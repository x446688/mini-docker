#!/bin/bash
LIBDIR=$(test -d /lib64/ && echo "lib64" || echo "lib" )
CONTAINER_DIR=$1
if !(ls ${CONTAINER_DIR}); then
    mkdir -p ${CONTAINER_DIR}/usr/bin ${CONTAINER_DIR}/lib ${CONTAINER_DIR}/${LIBDIR} ${CONTAINER_DIR}/var/log/
    cp /usr/bin/busybox ${CONTAINER_DIR}/usr/bin
    ldd /usr/bin/busybox | awk '/ => / { print $3 }' > tmp.txt
    FILE="tmp.txt"
    while read -r line; do
        cp "$line" ${CONTAINER_DIR}/${LIBDIR}/.
    done <$FILE
    rm tmp.txt
    cp /${LIBDIR}/ld-linux-*.so.* ${CONTAINER_DIR}/${LIBDIR}
fi