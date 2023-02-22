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

#define NR_pack 2L
#define NR_ch 128UL
#define NR_cpu 8

typedef struct {
	short *mat;
	char *vec;
	long nr;
	long nc;
	void *dest;
	char *mask;
} parStruct;

int asmfunc(short *mat, char *vec, long nr, long nc, void *dest, char *mask);
void f16tof32(void *dest, void *src);

/* Launch a function on lcore. 8< */
static int
lcore_math(void *arg)
{
	unsigned lcore_id;
	parStruct *params = arg;

	lcore_id = rte_lcore_id();
	printf("hello from core %u\n", lcore_id);
	asmfunc(params->mat, params->vec, params->nr, params->nc, params->dest, params->mask);

	return 0;
}
/* >8 End of launching function on lcore. */

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;
	char *vec, *mask;
	unsigned long *lmask;
	short mat[16*16*2*1024];
	char dest[16*1024*2] __attribute__ ((aligned (64)));
	float dest2[16*1024];
	int i, j, k, n, p, p2, ncores;
	parStruct params[RTE_MAX_LCORE];

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

	/* Create the data mask */
	n = (NR_pack + 31) / 32 * 8;
	mask = malloc(n);
	lmask = (unsigned long *) mask;
	for (i=0; i<n/8; i++) {
		lmask[i] = 0x0000000000000006;
	}

	/* Create the data buffer */
	vec = rte_malloc_socket(NULL, NR_pack*1024*16, 0x40, rte_socket_id());
	if (vec == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init data buffer\n");

	// transposed matrix
	for (k=0; k<1024; k++)
		for (j=0; j<16; j++)
			for (i=0; i<16; i++) {
				// p: real, p2:imag
				p = ((k * 16 + j) * 16 + i) * 2;
				p2 = p + 1;
//				if (i == j && (k % 2) == 0) {
				if (i == j) {
					mat[p] = 1;
					mat[p2] = 0;
				} else {
					mat[p] = 0;
					mat[p2] = 0;
				}
			}

	for (i=0; i<NR_pack; i++)
		for (j=0; j<1024; j++)
			for (k=0; k<16; k++) {
				p = (i * 1024 + j) * 16 + k;
//				vec[p] = 1;
				vec[p] = j % 256;
//				if (j % 2 != 0)
//					vec[p] = 0;
//				else
//					vec[p] = k * 15;
			}

	/* Launches the function on each lcore. 8< */
	lcore_id = rte_get_next_lcore(-1, 1, 0);
	i = 1;
	while (lcore_id < RTE_MAX_LCORE && i < NR_cpu) {
		params[i].mat = mat + i * NR_ch * 512;
		params[i].vec = vec + i * NR_ch * 16;
		params[i].nr = NR_pack;
		params[i].nc = NR_ch;
		params[i].dest = dest + i * NR_ch * 16 * 2;
		params[i].mask = mask + (i * NR_ch / 512);
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
	params[0].mask = mask;
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
	f16tof32(dest2, dest);
	printf("Dest:\n");
	for (i=0; i<1024; i++) {
		printf("channel %4d: ", i);
		for (j=0; j<16; j++) {
			p = i * 16 + j;
			printf(" %.2f", dest2[p]);
		}
		printf("\n");
	}

	/* clean up the EAL */
	free(mask);
	rte_free(vec);
	rte_eal_cleanup();

	return 0;
}
