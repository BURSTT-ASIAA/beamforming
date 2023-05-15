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

#define NR_sum 400UL
#define NR_run 100000UL
#define NR_REPEAT 5

typedef struct {
	short *mat;
	char *vec;
	long nr;
	int repeat;
	char *dest;
	double min_t, max_t, avg_t;
} parStruct;

void multiply_vnni(short *mat, char *vec, long nr, char *dest);
void multiply_fp16(short *mat, char *vec, long nr, char *dest);

/* Launch a function on lcore. 8< */
static int
lcore_math(void *arg)
{
	parStruct *params = arg;
	struct timeval start_t, end_t;
	double time_used, min_t, max_t, avg_t;
	char *vec;
	char *dest;
	int i;

	min_t = DBL_MAX;
	max_t = DBL_MIN;
	avg_t = 0.0;
	vec = params->vec;
	dest = params->dest;
	for (i=0; i<params->repeat; i++) {
		gettimeofday(&start_t, NULL);
		multiply_vnni(params->mat, vec, params->nr, dest);
		gettimeofday(&end_t, NULL);

		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
		if (min_t > time_used) min_t = time_used;
		if (max_t < time_used) max_t = time_used;
		avg_t += time_used;

		vec += 64 * params->nr;
		dest += 64;
	}

	params->min_t = min_t;
	params->max_t = max_t;
	params->avg_t = avg_t / params->repeat;

	return 0;
}
/* >8 End of launching function on lcore. */

static int
lcore_math_fp16(void *arg)
{
	parStruct *params = arg;
	struct timeval start_t, end_t;
	double time_used, min_t, max_t, avg_t;
	char *vec;
	char *dest;
	int i;

	min_t = DBL_MAX;
	max_t = DBL_MIN;
	avg_t = 0.0;
	vec = params->vec;
	dest = params->dest;
	for (i=0; i<params->repeat; i++) {
		gettimeofday(&start_t, NULL);
		multiply_fp16(params->mat, vec, params->nr, dest);
		gettimeofday(&end_t, NULL);

		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
		if (min_t > time_used) min_t = time_used;
		if (max_t < time_used) max_t = time_used;
		avg_t += time_used;

		vec += 64 * params->nr;
		dest += 32;
	}

	params->min_t = min_t;
	params->max_t = max_t;
	params->avg_t = avg_t / params->repeat;

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
	clock_t start, end;
	double cpu_time_used, time_used;
	struct timeval start_t, end_t;
	double min_t, max_t, avg_t;

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
			mat[p] = 0;
			mat[p + 1] = 0;
		}

	// set parameters
	params.mat = mat;
	params.vec = vec;
	params.nr = NR_sum;
	params.repeat = NR_run;
	params.dest = dest;

	printf("\n====== VNNI ======\n");
	for (p=0; p<NR_REPEAT; p++) {
		/* performance statistic */
		start = clock();
		gettimeofday(&start_t, NULL);

		/* Launches the function */
		lcore_math(&params);

		printf("\n====== test #%d ======\n", p+1);
		end = clock();
		gettimeofday(&end_t, NULL);
		cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
		printf("Total CPU time: %.3f   Real time: %3f\n", cpu_time_used, time_used);

		min_t = params.min_t;
		max_t = params.max_t;
		avg_t = params.avg_t;
		printf("min:avg:max %.3f:%.3f:%.3f\n", min_t*1000, avg_t*1000, max_t*1000);
	}

	printf("\n====== FP16 ======\n");
	for (p=0; p<NR_REPEAT; p++) {
		/* performance statistic */
		start = clock();
		gettimeofday(&start_t, NULL);

		/* Launches the function */
		lcore_math_fp16(&params);

		printf("\n====== test #%d ======\n", p+1);
		end = clock();
		gettimeofday(&end_t, NULL);
		cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
		printf("Total CPU time: %.3f   Real time: %3f\n", cpu_time_used, time_used);

		min_t = params.min_t;
		max_t = params.max_t;
		avg_t = params.avg_t;
		printf("min:avg:max %.3f:%.3f:%.3f\n", min_t*1000, avg_t*1000, max_t*1000);
	}

	/* clean up the EAL */
	free(vec);
	free(dest);

	return 0;
}
