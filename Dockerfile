FROM ubuntu:latest
RUN <<EOF
    apt update -y
    apt install -y gcc make tree gdb
EOF
WORKDIR /root
CMD ["/bin/bash"]
