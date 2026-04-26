FROM debian:latest
RUN <<EOF
    apt update -y
    apt install -y gcc make tree gdb libcap-dev git indent busybox
EOF
WORKDIR /root/mini-docker
CMD ["/bin/bash"]
