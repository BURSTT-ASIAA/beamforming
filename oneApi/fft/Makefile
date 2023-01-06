# Makefile for GNU Make

all: fft_mkl

MKL_COPTS = -DMKL_ILP64  -I"${MKLROOT}/include"
MKL_LIBS = -L${MKLROOT}/lib/intel64 -lmkl_sycl -lmkl_intel_ilp64 -lmkl_sequential -lmkl_core -lsycl -lOpenCL -lpthread -lm -ldl
#MY_COPTS = -xCore-AVX512
MY_COPTS =

DPCPP_OPTS = $(MKL_COPTS) -fsycl-device-code-split=per_kernel $(MKL_LIBS)

fft_mkl: fft_mkl.cpp
	icpx $(MY_COPTS) $< -fsycl -o $@ $(DPCPP_OPTS)

clean:
	-rm -f fft_mkl

.PHONY: clean all
