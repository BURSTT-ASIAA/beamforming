#include "mkl_dfti.h"
#include <sys/time.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

#define N_FFT 16
#define N_REPEAT 50000000L

const char *fpga_in = "/mnt/fpga2/fpga.bin";
const char *fpga_out = "/mnt/fpga3/fpga.bin";

int main() {
	MKL_Complex8 *p_in, *p_out, *p_data, *p_data2;
	DFTI_DESCRIPTOR_HANDLE desc_handle = NULL;
	MKL_LONG status, thr_limit;
	struct timeval start_t, end_t;
	float time_used, scale;
	int fd, i;
	struct stat sb_in, sb_out;

	/* malloc */
//	p_in = (MKL_Complex8 *)malloc(sizeof(MKL_Complex8) * N_FFT * N_REPEAT);
//	p_out = (MKL_Complex8 *)malloc(sizeof(MKL_Complex8) * N_FFT * N_REPEAT);

	/* open hugepage */
	fd = open(fpga_in, O_RDWR);
	if (fd < 0) {
		std::cerr << "Cannot open fpga file: " << fpga_in << std::endl;
		return 0;
	}
	if (fstat(fd, &sb_in) < 0) {
		std::cerr << "Cannot fstat fpga file: " << fpga_in << std::endl;
		return 0;
	}
	p_in = (MKL_Complex8 *)mmap(NULL, sb_in.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
    if (p_in == MAP_FAILED) {
		std::cerr << "Cannot mmap data buffer" << std::endl;
		return 0;
	}

	fd = open(fpga_out, O_RDWR);
	if (fd < 0) {
		std::cerr << "Cannot open fpga file: " << fpga_out << std::endl;
		return 0;
	}
	if (fstat(fd, &sb_out) < 0) {
		std::cerr << "Cannot fstat fpga file: " << fpga_out << std::endl;
		return 0;
	}
	p_out = (MKL_Complex8 *)mmap(NULL, sb_out.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
    if (p_out == MAP_FAILED) {
		std::cerr << "Cannot mmap data buffer" << std::endl;
		return 0;
	}

	/* ...put values into p_in[i] */
	for (i=0; i<N_FFT*2; i++) p_in[i] = {1.0, 0.0};
	scale = 1.0 / sqrtf(N_FFT);

	/* out-of-place FFT */
	status = DftiCreateDescriptor(&desc_handle, DFTI_SINGLE, DFTI_COMPLEX, 1, N_FFT);
//	status = DftiSetValue(desc_handle, DFTI_FORWARD_SCALE, scale);
//	status = DftiSetValue(desc_handle, DFTI_BACKWARD_SCALE, scale);
	status = DftiSetValue(desc_handle, DFTI_NUMBER_OF_TRANSFORMS, N_REPEAT);
	status = DftiSetValue(desc_handle, DFTI_INPUT_DISTANCE, N_FFT);
	status = DftiSetValue(desc_handle, DFTI_OUTPUT_DISTANCE, N_FFT);
	status = DftiSetValue(desc_handle, DFTI_PLACEMENT, DFTI_NOT_INPLACE);
	status = DftiSetValue(desc_handle, DFTI_THREAD_LIMIT, 0L);
	status = DftiGetValue(desc_handle, DFTI_THREAD_LIMIT, &thr_limit);
	std::cout << "DFTI_THREAD_LIMIT: " << thr_limit << std::endl;
	status = DftiCommitDescriptor(desc_handle);
	gettimeofday(&start_t, NULL);
	status = DftiComputeForward(desc_handle, p_in, p_out);
	gettimeofday(&end_t, NULL);
	status = DftiFreeDescriptor(&desc_handle);
	time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;

	/* result is p_out */
	std::cout << "Out-of-place FFT:";
	for (i=N_FFT; i<N_FFT*2; i++) {
		std::cout << "(" << p_out[i].real;
		std::cout << " " << p_out[i].imag << ")";
	}
	std::cout << std::endl;
	std::cout << "Total time used: " << time_used << "s\n";

	/* in-place FFT */
	status = DftiCreateDescriptor(&desc_handle, DFTI_SINGLE, DFTI_COMPLEX, 1, N_FFT);
	status = DftiSetValue(desc_handle, DFTI_PLACEMENT, DFTI_INPLACE);
	status = DftiSetValue(desc_handle, DFTI_NUMBER_OF_TRANSFORMS, N_REPEAT);
	status = DftiSetValue(desc_handle, DFTI_INPUT_DISTANCE, N_FFT);
	status = DftiSetValue(desc_handle, DFTI_OUTPUT_DISTANCE, N_FFT);
	status = DftiSetValue(desc_handle, DFTI_THREAD_LIMIT, 0L);
	status = DftiGetValue(desc_handle, DFTI_THREAD_LIMIT, &thr_limit);
	std::cout << "DFTI_THREAD_LIMIT: " << thr_limit << std::endl;
	status = DftiCommitDescriptor(desc_handle);
	gettimeofday(&start_t, NULL);
	status = DftiComputeForward(desc_handle, p_in);
	gettimeofday(&end_t, NULL);
	status = DftiFreeDescriptor(&desc_handle);
	time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;

	/* result is p_in */
	std::cout << "In-place FFT:";
	for (i=N_FFT; i<N_FFT*2; i++) {
		std::cout << "(" << p_in[i].real;
		std::cout << " " << p_in[i].imag << ")";
	}
	std::cout << std::endl;
	std::cout << "Total time used: " << time_used << "s\n";

//	free(p_in);
//	free(p_out);
	munmap(p_in, sb_in.st_size);
	munmap(p_out, sb_out.st_size);
}
