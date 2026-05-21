FROM debian:latest
RUN apt update -y
RUN apt install -y gcc make tree gdb libcap-dev git indent busybox g++
WORKDIR /root/mini-docker
COPY . /root/mini-docker/
ENTRYPOINT ["make", "all"]