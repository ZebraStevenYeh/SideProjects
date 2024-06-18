FROM --platform=linux/amd64 ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive LANG=en_US.UTF-8

RUN apt update
# Informational list of build-essential packages
RUN apt install -y build-essential
RUN apt install -y gcc
RUN apt install -y g++
RUN apt install -y gcc-multilib
RUN apt install -y make
RUN apt install -y cmake
RUN apt install -y openssl
RUN apt install -y vim
RUN apt install -y nano
# Intel ASL compiler/decompiler (also provided by acpica-tools)
RUN apt install -y iasl
# General-purpose x86 assembler
RUN apt install -y nasm
# Universally Unique ID library (headers and static libraries)
RUN apt install -y uuid-dev
# Ubuntu 20.04 python command is 'python3' but edk2 tools use 'python'
RUN apt install -y python-is-python3
RUN apt install -y git
RUN apt install -y wget 