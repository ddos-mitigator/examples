# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021 BIFIT Mitigator Team <info@mitigator.ru>

CFLAGS = -emit-llvm -O2 -g -fno-stack-protector -Wall -Wextra

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
OUTDIR = build

.PHONY: all

build_folder := $(shell mkdir -p $(OUTDIR))

all: $(OBJS)

mitigator_bpf.h:
	wget --no-clobber https://docs.mitigator.ru/v21.04/kb/bpf/mitigator_bpf.h

%.o: %.c mitigator_bpf.h
	clang -c -I. $< $(CFLAGS) -o- | \
	llc -filetype obj -march=bpf -o $(OUTDIR)/$@
