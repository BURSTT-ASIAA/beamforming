#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <float.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define NR_sum 1UL
#define NR_run 1UL

typedef struct {
	short *mat;
	char *vec;
	long nr;
	int repeat;
	char *dest;
} parStruct;

void multiply_vnni(short *mat, char *vec, long nr, char *dest);
void multiply_fp16(short *mat, char *vec, long nr, char *dest);
void f32tof16x(char *dest, float *src);
void f16tof32x(float *dest, char *src);

/* Launch a function on lcore. 8< */
static int
lcore_math(void *arg)
{
	parStruct *params = arg;
	char *vec;
	char *dest;
	int i;

	vec = params->vec;
	dest = params->dest;
	for (i=0; i<params->repeat; i++) {
		multiply_vnni(params->mat, vec, params->nr, dest);
		vec += 64 * params->nr;
		dest += 64;
	}

	return 0;
}
/* >8 End of launching function on lcore. */

static int
lcore_math_fp16(void *arg)
{
	parStruct *params = arg;
	char *vec;
	char *dest;
	int i;

	vec = params->vec;
	dest = params->dest;
	for (i=0; i<params->repeat; i++) {
		multiply_fp16(params->mat, vec, params->nr, dest);
		vec += 64 * params->nr;
		dest += 32;
	}

	return 0;
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
	char *vec;
	short mat[16*16*2];
	char *dest;
	int i, j, p;
	parStruct params;
	float *fptr, matf[16*16*2], vecf[16*2];
	short *sptr;
	_Float16 *hptr;

	/* Create the data buffer */
	vec = malloc(64 * NR_run * NR_sum);
	if (vec == NULL) {
		printf("Cannot mmap data buffer\n");
		return 0;
	}

//	dest = aligned_alloc(64 * NR_run, 16);
	dest = malloc(64 * NR_run);
	if (dest == NULL) {
		printf("Cannot init sum buffer\n");
		return 0;
	}

	// transposed matrix
	for (j=0; j<16; j++)
		for (i=0; i<16; i++) {
			// p: real, p2:imag
			p = (j * 16 + i) * 2;
			if (i == j)
				mat[p] = 1;
			else
				mat[p] = 0;
			mat[p + 1] = 0;
		}

	// input data
	sptr = (short *)vec;
	for (j=0; j<NR_run*NR_sum; j++)
		for (i=0; i<16; i++) {
			// p: real, p2:imag
			p = (j * 16 + i) * 2;
			sptr[p] = i;
			sptr[p + 1] = 0;
		}

	// set parameters
	params.mat = mat;
	params.vec = vec;
	params.nr = NR_sum;
	params.repeat = NR_run;
	params.dest = dest;

	/* Launches the function */
	lcore_math(&params);

	// Print output
	fptr = (float *)dest;
	printf("=== VNNI output ===\n");
	for (j=0; j<NR_run; j++)
		for (i=0; i<16; i++) {
			printf(" %.0f", fptr[i]);
		}
	printf("\n");

	// transposed matrix in FP16
	hptr = (_Float16 *)mat;
	for (j=0; j<16; j++)
		for (i=0; i<16; i++) {
			// p: real, p2:imag
			p = (j * 16 + i) * 2;
			if (i == j)
				hptr[p] = 1;
			else
				hptr[p] = 0;
			hptr[p + 1] = 0;
		}

	// input data
	hptr = (_Float16 *)vec;
	for (j=0; j<NR_run*NR_sum; j++)
		for (i=0; i<16; i++) {
			// p: real, p2:imag
			p = (j * 16 + i) * 2;
			hptr[p] = i;
			hptr[p + 1] = 0;
		}

	/* Launches the function */
	lcore_math_fp16(&params);

	// Print output
	hptr = (_Float16 *)dest;
	printf("=== FP16 output ===\n");
	for (j=0; j<NR_run; j++)
		for (i=0; i<16; i++) {
			printf(" %.0f", hptr[i]);
		}
	printf("\n");

	/* clean up the EAL */
	free(vec);
	free(dest);

	return 0;
}
