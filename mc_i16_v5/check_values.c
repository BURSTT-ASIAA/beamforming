#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <time.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_malloc.h>

#define BUFFER_SIZE 1024*16*400*1024U // 6.4G
#define NR_pack 100L
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

	lcore_id = rte_lcore_id();
	printf("hello from core %u\n", lcore_id);
	asmfunc(params->mat, params->vec, params->nr, params->nc, params->dest);

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
	float dest[16*1024] __attribute__ ((aligned (64)));
	int i, j, k, n, p, p2, ncores;
	parStruct params[RTE_MAX_LCORE];

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

	/* Create the data buffer */
	vec = rte_malloc_socket(NULL, NR_pack*1024*16, 0x40, rte_socket_id());
	if (vec == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init data buffer\n");

	// transposed matrix
	for (j=0; j<16; j++) {
		for (i=0; i<16; i++) {
			// p: real, p2:imag
			p = i * 32 + j * 2;
			p2 = i * 32 + j * 2 + 1;
			if (i == j) {
				mat[p] = 1;
				mat[p2] = 0;
			} else {
				mat[p] = 0;
				mat[p2] = 0;
			}
		}
	}

	for (i=0; i<NR_pack; i++)
		for (j=0; j<1024; j++)
			for (k=0; k<16; k++) {
				p = (i * 1024 + j) * 16 + k;
				vec[p] = k * 15;
			}

	/* Launches the function on each lcore. 8< */
	lcore_id = rte_get_next_lcore(-1, 1, 0);
	i = 1;
	while (lcore_id < RTE_MAX_LCORE && i < 16) {
		params[i].mat = mat;
		params[i].vec = vec + i * NR_ch * 16;
		params[i].nr = NR_pack;
		params[i].nc = NR_ch;
		params[i].dest = dest + i * NR_ch * 16;
		rte_eal_remote_launch(lcore_math, &params[i], lcore_id);
		i++;
		lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
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

	/* print info */
	printf("Matrix:\n");
	for (j=0; j<16; j++) {
		for (i=0; i<32; i++) {
			p = j*32 + i;
			printf(" %hd", mat[p]);
		}
		printf("\n");
	}
	printf("\nVector:");
	for (i=0; i<16; i++) {
		printf(" %hhx", vec[i]);
	}
	printf("\n");
	printf("Dest:\n");
	for (i=0; i<1024; i++) {
		printf("channel %4d: ", i);
		for (j=0; j<16; j++) {
			p = i * 16 + j;
			printf(" %.2f", dest[p]);
		}
		printf("\n");
	}

	/* clean up the EAL */
	rte_free(vec);
	rte_eal_cleanup();

	return 0;
}
