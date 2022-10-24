#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <float.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_malloc.h>

#define NR_pack 400*1000UL // 6.4G
#define NR_sum 400UL  // integral count
#define NR_run 1000UL // NR_pack / NR_sum
#define NR_ch 64UL
#define NR_cpu 16 // 1024 / NR_ch

typedef struct {
	short *mat;
	char *vec;
	long nr;
	long nc;
	float *dest;
	double time_used;
} parStruct;

int asmfunc(short *mat, char *vec, long nr, long nc, float *dest);

/* Launch a function on lcore. 8< */
static int
lcore_math(void *arg)
{
	unsigned lcore_id;
	parStruct *params = arg;
	struct timeval start_t, end_t;
	double time_used;

	gettimeofday(&start_t, NULL);
	asmfunc(params->mat, params->vec, params->nr, params->nc, params->dest);
	gettimeofday(&end_t, NULL);
	time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
	params->time_used = time_used;

	return 0;
}
/* >8 End of launching function on lcore. */

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;
	char *vec;
	short mat[16*16*2*1024];
	float *dest;
	int i, j, k, p, ncores;
	parStruct params[RTE_MAX_LCORE];
	clock_t start, end;
	double cpu_time_used;
	double min_t[RTE_MAX_LCORE], max_t[RTE_MAX_LCORE], avg_t[RTE_MAX_LCORE];

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

	/* Create the data buffer */
	vec = rte_malloc_socket(NULL, 1024*16*NR_pack, 0x40, rte_socket_id());
	if (vec == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init data buffer\n");
	dest = rte_malloc_socket(NULL, 1024*64*NR_run, 0x40, rte_socket_id());
	if (dest == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init sum buffer\n");

	// transposed matrix
	for (k=0; k<1024; k++)
		for (j=0; j<16; j++)
			for (i=0; i<16; i++) {
				// p: real, p2:imag
				p = ((k * 16 + j) * 16 + i) * 2;
				mat[p] = 0;
				mat[p + 1] = 0;
			}

	/* performance statistic */
	for (i=0; i<RTE_MAX_LCORE; i++) {
		min_t[i] = DBL_MAX;
		max_t[i] = DBL_MIN;
		avg_t[i] = 0.0;
	}

	/* Launches the function on each lcore. 8< */
	start = clock();

	for (j=0; j<NR_run; j++) {
		i = 1;
		RTE_LCORE_FOREACH_WORKER(lcore_id) {
			params[i].mat = mat + i * NR_ch * 512;
			params[i].vec = vec + (i * NR_ch + 1024 * NR_sum * j) * 16;
			params[i].nr = NR_sum;
			params[i].nc = NR_ch;
			params[i].dest = dest + (i * NR_ch + 1024 * j) * 16;
			rte_eal_remote_launch(lcore_math, &params[i], lcore_id);
			i++;
			if (i >= NR_cpu) break;
		}
		ncores = i;

		/* call it on main lcore too */
		params[0].mat = mat;
		params[0].vec = vec + 1024 * 16 * NR_sum * j;
		params[0].nr = NR_sum;
		params[0].nc = NR_ch;
		params[0].dest = dest + 1024 * 16 * j;
		lcore_math(&params[0]);
		/* >8 End of launching the function on each lcore. */

		rte_eal_mp_wait_lcore();

		/* performance statistic */
		for (i=0; i<ncores; i++) {
			float t = params[i].time_used;
			if (min_t[i] > t) min_t[i] = t;
			if (max_t[i] < t) max_t[i] = t;
			avg_t[i] += t;
		}
	}

	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("Total lcores: %d  Total CPU time: %.3f  Average time: %.3f\n",
			ncores, cpu_time_used, cpu_time_used/ncores);

	for (i=0; i<ncores; i++) {
		printf("DPDK lcore #%2d  min:avg:max %.3f:%.3f:%.3f\n", i,
				min_t[i]*NR_run, avg_t[i]/NR_run*NR_run, max_t[i]*NR_run);
	}

	/* clean up the EAL */
	rte_free(vec);
	rte_eal_cleanup();

	return 0;
}
