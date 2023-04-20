# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# binary name
APP = integral

# all source are stored in SRCS-y
SRCS-y := integral.c
ASM_FILE = build/asmfunc_vnni.o
ASM_SRC = asmfunc_vnni.asm

PKGCONF ?= pkg-config

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

all: shared
.PHONY: shared static
shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)
static: build/$(APP)-static
	ln -sf $(APP)-static build/$(APP)
asm: $(ASM_FILE)

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS = -lm
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC = $(shell $(PKGCONF) --static --libs libdpdk)

ifeq ($(MAKECMDGOALS),static)
# check for broken pkg-config
ifeq ($(shell echo $(LDFLAGS_STATIC) | grep 'whole-archive.*l:lib.*no-whole-archive'),)
$(warning "pkg-config output list does not contain drivers between 'whole-archive'/'no-whole-archive' flags.")
$(error "Cannot generate statically-linked binaries with this version of pkg-config")
endif
endif

CFLAGS += -DALLOW_EXPERIMENTAL_API -lrt

build/$(APP)-shared: $(SRCS-y) Makefile $(PC_FILE) $(ASM_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) build/asmfunc_vnni.o build/fp16.o -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/$(APP)-static: $(SRCS-y) Makefile $(PC_FILE) $(ASM_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) build/asmfunc_vnni.o build/fp16.o -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP) build/$(APP)-static build/$(APP)-shared build/asmfunc_vnni.o
	test -d build && rmdir -p build || true

$(ASM_FILE): Makefile $(ASM_SRC) | build
	nasm -f elf64 -o $@ $(ASM_SRC)

mqueue: mqueue.c
	$(CC) -o $@ mqueue.c -lrt

fpga_sim: fpga_sim.c
	$(CC) -o $@ fpga_sim.c -lrt

ramdisk: ramdisk.c build/fp16.o
	$(CC) -o $@ ramdisk.c build/fp16.o

build/fp16.o: fp16.asm
	nasm -f elf64 -o $@ fp16.asm

intensity: intensity.c
	$(CC) -o $@ intensity.c

voltage: voltage.c
	$(CC) -o $@ voltage.c
