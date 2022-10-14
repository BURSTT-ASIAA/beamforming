#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_malloc.h>

#define BUFFER_SIZE 1024*16*400*100UL // 640M
#define nvec 1024*400*100UL

typedef struct {
	short *mat;
	char *vec;
	long nr;
	long nc;
	float *dest;
} parStruct;

int asmfunc(short *mat, char *vec, long n, float *dest);

/* Launch a function on lcore. 8< */
static int
lcore_math(void *arg)
{
	unsigned lcore_id;
	parStruct *params = arg;
	struct timeval start_t, end_t;
	double time_used;

	lcore_id = rte_lcore_id();
	gettimeofday(&start_t, NULL);
	asmfunc(params->mat, params->vec, params->nr, params->dest);
	gettimeofday(&end_t, NULL);
	time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
	printf("CPU core #%u  Real time: %.3f\n", lcore_id, time_used);

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
	short mat[16*16*2];
	float dest[16*RTE_MAX_LCORE] __attribute__ ((aligned (64)));
	int i, j, n, p, p2, ncores;
	parStruct params[RTE_MAX_LCORE];
	clock_t start, end;
	double cpu_time_used;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

	/* Create the data buffer */
	vec = rte_malloc_socket(NULL, BUFFER_SIZE, 0x40, rte_socket_id());
	if (vec == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init data buffer %d\n", i);

	// transposed matrix
	for (j=0; j<16; j++) {
		for (i=0; i<16; i++) {
			// p: real, p2:imag
			p = i * 32 + j * 2;
			p2 = i * 32 + j * 2 + 1;
			if (i == j) {
				mat[p] = 1;
				mat[p2] = 1;
			} else {
				mat[p] = 0;
				mat[p2] = 0;
			}
		}
	}

	/* Launches the function on each lcore. 8< */
	i = 1;
	start = clock();
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		params[i].mat = mat;
		params[i].vec = vec;
		params[i].nr = nvec;
		params[i].dest = dest + i * 16;
		rte_eal_remote_launch(lcore_math, &params[i], lcore_id);
		i++;
	}
	ncores = i;

	/* call it on main lcore too */
	params[0].mat = mat;
	params[0].vec = vec;
	params[0].nr = nvec;
	params[0].dest = dest;
	lcore_math(&params[0]);
	/* >8 End of launching the function on each lcore. */

	rte_eal_mp_wait_lcore();
	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("Total CPU time: %.3f\n", cpu_time_used);

	/* clean up the EAL */
	rte_free(vec);
	rte_eal_cleanup();

	return 0;
}
