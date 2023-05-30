# syntax=docker/dockerfile:1
FROM ubuntu:focal

MAINTAINER Johnathan Leung <johnathan.leung@eosnation.io>

COPY . /app
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Hong_Kong
RUN apt-get update -q
RUN apt-get install -yqq cmake git build-essential m4 texinfo clang gcc-10 g++-10
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100 --slave /usr/bin/g++ g++ /usr/bin/g++-10 --slave /usr/bin/gcov gcov /usr/bin/gcov-10
WORKDIR /app
RUN git submodule update --init --recursive
RUN mkdir build
WORKDIR /app/build
RUN make -j "$(nproc)"
RUN mkdir /usr/bin/eos-evm-node
RUN cp ./cmd/eos-evm-node /usr/bin/eos-evm-node
WORKDIR /app
RUN eos-evm-node --plugin block_conversion_plugin --plugin blockchain_plugin --nocolor 1 --verbosity=5 --chain-data /usr/bin/eos-evm --genesis-json=./genesis.json
