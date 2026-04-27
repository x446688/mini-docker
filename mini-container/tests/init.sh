mkdir -p /tmp/mini-docker/usr/bin /tmp/mini-docker/lib /tmp/mini-docker/lib64 /tmp/mini-docker/var/log/
cp /usr/bin/busybox /tmp/mini-docker/usr/bin
ldd /usr/bin/busybox | awk '/ => / { print $1 }' > tmp.txt
file="tmp.txt"
while read -r line; do
    cp /lib64/"$line" /tmp/mini-docker/lib64/.
done <$file
rm tmp.txt
cp /lib64/ld-linux-x86-64.so.2 /tmp/mini-docker/lib64
