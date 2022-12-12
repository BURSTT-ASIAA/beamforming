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
#define NR_ch 128UL // must be multiple of 16
#define NR_cpu 8 // 1024 / NR_ch
#define NR_FPGA 4
#define NR_REPEAT 5

typedef struct {
	short *mat;
	char *vec;
	long nr;
	long nc;
	int repeat;
	float *dest;
	char *mask;
	double min_t, max_t, avg_t;
	unsigned lcore_id;
} parStruct;

int asmfunc(short *mat, char *vec, long nr, long nc, float *dest, char *mask);

const unsigned mem_node[4] = {0, 1, 2, 3};
const char *fpga_fn[] = {
	"/mnt/ens2f0np0/fpga.bin",
	"/mnt/ens2f1np1/fpga.bin",
	"/mnt/ens5f0np0/fpga.bin",
	"/mnt/ens5f1np1/fpga.bin"
};

/* Launch a function on lcore. 8< */
static int
lcore_math(void *arg)
{
	unsigned lcore_id;
	parStruct *params = arg;
	struct timeval start_t, end_t;
	double time_used, min_t, max_t, avg_t;
	char *vec;
	float *dest;
	int i;

	min_t = DBL_MAX;
	max_t = DBL_MIN;
	avg_t = 0.0;
	vec = params->vec;
	dest = params->dest;
	for (i=0; i<params->repeat; i++) {
		gettimeofday(&start_t, NULL);
		asmfunc(params->mat, vec, params->nr, params->nc, dest, params->mask);
		gettimeofday(&end_t, NULL);

		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
		if (min_t > time_used) min_t = time_used;
		if (max_t < time_used) max_t = time_used;
		avg_t += time_used;

		vec += 1024 * 16 * params->nr;
		dest += 1024 * 16;
	}

	params->min_t = min_t;
	params->max_t = max_t;
	params->avg_t = avg_t / params->repeat;
	params->lcore_id = rte_lcore_id();

	return 0;
}
/* >8 End of launching function on lcore. */

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
	int fd, ret;
	unsigned lcore_id, socket_id;
	char *vec[NR_FPGA], *mask[NR_FPGA];
	unsigned long *lmask;
	short mat[16*16*2*1024];
	float *dest[NR_FPGA];
	int i, j, k, p, ncores;
	parStruct params[RTE_MAX_LCORE] __attribute__ ((aligned (64)));
	clock_t start, end;
	double cpu_time_used, time_used;
	struct timeval start_t, end_t;
	double min_t, max_t, avg_t;
	struct stat sb[NR_FPGA];

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

	/* Create the data buffer */
	p = (NR_pack + 31) / 32 * 8;
	for (i=0; i<NR_FPGA; i++) {
		fd = open(fpga_fn[i], O_RDONLY);
		if (fd < 0)
			rte_exit(EXIT_FAILURE, "Cannot open fpga file: %s\n", fpga_fn[i]);

		if (fstat(fd, &sb[i]) < 0)
			rte_exit(EXIT_FAILURE, "Cannot fstat fpga file: %s\n", fpga_fn[i]);

		vec[i] = mmap(NULL, sb[i].st_size, PROT_READ, MAP_SHARED, fd, 0);
		close(fd);
		if (vec[i] == MAP_FAILED)
			rte_exit(EXIT_FAILURE, "Cannot mmap data buffer\n");

		dest[i] = rte_malloc_socket(NULL, 1024*64*NR_run, 0x40, mem_node[i]);
		if (dest[i] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init sum buffer\n");

		mask[i] = malloc(p);
		lmask = (long *) mask[i];
		for (j=0; j<p/8; j++) {
			lmask[j] = 0x0000000000000004;
		}
	}

	// transposed matrix
	for (k=0; k<1024; k++)
		for (j=0; j<16; j++)
			for (i=0; i<16; i++) {
				// p: real, p2:imag
				p = ((k * 16 + j) * 16 + i) * 2;
				mat[p] = 0;
				mat[p + 1] = 0;
			}

	for (p=0; p<NR_REPEAT; p++) {
		/* performance statistic */
		start = clock();
		gettimeofday(&start_t, NULL);

		i = 0;
		j = 0;
		k = 0;

		/* Launches the function on each lcore. 8< */
		RTE_LCORE_FOREACH_WORKER(lcore_id) {
			params[i].mat = mat + j * NR_ch * 512;
			params[i].vec = vec[k] + j * NR_ch * 16;
			params[i].nr = NR_sum;
			if ((j+1) * NR_ch <= 1024)
				params[i].nc = NR_ch;
			else
				params[i].nc = 1024 - j * NR_ch;
			params[i].repeat = NR_run;
			params[i].dest = dest[k] + j * NR_ch * 16;
			params[i].mask = mask[k] + (j * NR_ch / 512);
			rte_eal_remote_launch(lcore_math, &params[i], lcore_id);
			i++;
			j++;
			if (j >= NR_cpu) {
				j = 0;
				k++;
			}
			if (k >= NR_FPGA) break;
		}
		ncores = i;

		/* call it on main lcore too */
//		params[0].mat = mat;
//		params[0].vec = vec + 1024 * 16 * NR_sum * j;
//		params[0].nr = NR_sum;
//		params[0].nc = NR_ch;
//		params[0].dest = dest + 1024 * 16 * j;
//		lcore_math(&params[0]);
		/* >8 End of launching the function on each lcore. */

		rte_eal_mp_wait_lcore();

		printf("\n====== test #%d ======\n", p+1);
		end = clock();
		gettimeofday(&end_t, NULL);
		cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
		printf("Total lcores: %d  Total CPU time: %.3f  Average time: %.3f  Real time: %3f\n",
				ncores, cpu_time_used, cpu_time_used/(ncores+1), time_used);

		for (i=0; i<ncores; i++) {
			lcore_id = params[i].lcore_id;
			socket_id = rte_lcore_to_socket_id(lcore_id);
			min_t = params[i].min_t;
			max_t = params[i].max_t;
			avg_t = params[i].avg_t;
			printf("%2d) DPDK lcore %3d(%d:%d)  min:avg:max %.3f:%.3f:%.3f\n", i,
					lcore_id, socket_id, mem_node[i/NR_cpu], min_t*1000, avg_t*1000, max_t*1000);
		}
	}

	/* clean up the EAL */
	for (i=0; i<NR_FPGA; i++) {
		munmap(vec[i], sb[i].st_size);
		rte_free(dest[i]);
		free(mask[i]);
	}
	rte_eal_cleanup();

	return 0;
}
