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

#define NR_pack 400*1000UL // 6.4G
#define NR_ch 64UL

typedef struct {
	short *mat;
	char *vec;
	long nr;
	long nc;
	float *dest;
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

	lcore_id = rte_lcore_id();
	gettimeofday(&start_t, NULL);
	asmfunc(params->mat, params->vec, params->nr, params->nc, params->dest);
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
	short mat[16*16*2*1024];
	float dest[16*1024] __attribute__ ((aligned (64)));
	int i, j, k, p, ncores;
	parStruct params[RTE_MAX_LCORE];
	clock_t start, end;
	double cpu_time_used;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

	/* Create the data buffer */
	vec = rte_malloc_socket(NULL, NR_pack*1024*16, 0x40, rte_socket_id());
	if (vec == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init data buffer %d\n", i);

	// transposed matrix
	for (k=0; k<1024; k++)
		for (j=0; j<16; j++)
			for (i=0; i<16; i++) {
				// p: real, p2:imag
				p = ((k * 16 + j) * 16 + i) * 2;
				mat[p] = 0;
				mat[p + 1] = 0;
			}

	/* Launches the function on each lcore. 8< */
	i = 1;
	start = clock();
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		params[i].mat = mat + i * NR_ch * 512;
		params[i].vec = vec + i * NR_ch * 16;
		params[i].nr = NR_pack;
		params[i].nc = NR_ch;
		params[i].dest = dest + i * NR_ch * 16;
		rte_eal_remote_launch(lcore_math, &params[i], lcore_id);
		i++;
		if (i >= 16) break;
	}
	ncores = i;

	/* call it on main lcore too */
	params[0].mat = mat;
	params[0].vec = vec;
	params[0].nr = NR_pack;
	params[0].nc = NR_ch;
	params[0].dest = dest;
	lcore_math(&params[0]);
	/* >8 End of launching the function on each lcore. */

	rte_eal_mp_wait_lcore();
	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("Total cores: %d  Total CPU time: %.3f\n", ncores, cpu_time_used);

	/* clean up the EAL */
	rte_free(vec);
	rte_eal_cleanup();

	return 0;
}
