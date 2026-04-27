FROM debian:latest
RUN apt update -y
RUN apt install -y gcc make tree gdb libcap-dev git indent busybox
WORKDIR /root/mini-docker
CMD ["/bin/bash"]
