#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
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

//#define NR_pack 400*1000UL // 6.4G
#define NR_sum 400UL  // integral count
#define NR_run 1000UL // NR_pack / NR_sum
#define NR_ch 128UL // must be multiple of 16
#define NR_cpu 8 // 1024 / NR_ch
#define NR_FPGA 4
#define NR_REPEAT 50
#define DATA_SIZE (8192 + 64) * 2
#define DATA_HEADER 64

typedef struct __attribute__ ((aligned (64))) {
	short *mat;
	char *vec;
	long nr;
	long nc;
	long repeat;
	float *dest;
	int *d_count;
	volatile bool filled;
} task_s;

int asmfunc(short *mat, char *vec, long nr, long nc, float *dest);

const unsigned mem_node[4] = {0, 1, 2, 3};
const char *fpga_fn[] = {
	"/mnt/fpga0/fpga.bin",
	"/mnt/fpga1/fpga.bin",
	"/mnt/fpga2/fpga.bin",
	"/mnt/fpga3/fpga.bin"
};
volatile int quit_signal = false;

/* Launch a function on lcore. 8< */
static int
lcore_integral(void *arg)
{
	task_s *params = arg;
	char *vec;
	float *dest;
	long i;
	float time_used;
	struct timeval start_t, end_t;

	while (!quit_signal) {
		if (!params->filled) {
			usleep(1000);
			continue;
		}

		gettimeofday(&start_t, NULL);
		vec = params->vec;
		dest = params->dest;
		for (i=0; i<params->repeat; i++) {
			asmfunc(params->mat, vec, params->nr, params->nc, dest);
			vec += DATA_SIZE * params->nr;
			dest += 1024 * 16;
		}
		params->filled = false;
		gettimeofday(&end_t, NULL);
		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
//		printf("lcores #%d,  Real time: %3f\n", rte_lcore_id(), time_used);
	}

	return 0;
}
/* >8 End of launching function on lcore. */

/* find a feee lcore or wait */
int find_lcore(task_s *tasks, int ncores) {
	int i;

	while (true) {
		for (i=0; i<ncores; i++)
			if (!tasks[i].filled) return i;
		usleep(1000);
	}
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
	int fd, ret;
	unsigned lcore_id, socket_id;
	char *vec[NR_FPGA];
	short mat[16*16*2*1024];
	float *dest[NR_FPGA];
	int i, j, k, p, ncores;
	task_s params[RTE_MAX_LCORE];
	struct stat sb[NR_FPGA];
	unsigned lcores[RTE_MAX_LCORE];
	long offset;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	/* Retrieve the data buffer */
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
			rte_exit(EXIT_FAILURE, "Cannot init integral buffer\n");
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

	/* Launches the function on each lcore. 8< */
	i = 0;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		params[i].filled = false;
		rte_eal_remote_launch(lcore_integral, &params[i], lcore_id);
		lcores[i] = lcore_id;
		i++;
	}
	ncores = i;

	for (p=0; p<NR_REPEAT; p++) {
		printf("\n====== Test #%d ======\n", p+1);
		for (k=0; k<NR_FPGA; k++) {
			printf("Process fpga#%d  cpus:", k);
			for (j=0; j<NR_cpu; j++) {
				i = find_lcore(params, ncores);
				if (j * NR_ch < 512)
					offset = DATA_HEADER;
				else
					offset = DATA_HEADER * 2;
				params[i].mat = mat + j * NR_ch * 512;
				params[i].vec = vec[k] + j * NR_ch * 16 + offset;
				params[i].nr = NR_sum;
				if ((j+1) * NR_ch <= 1024)
					params[i].nc = NR_ch;
				else
					params[i].nc = 1024 - j * NR_ch;
				params[i].repeat = NR_run;
				params[i].dest = dest[k] + j * NR_ch * 16;
				params[i].filled = true;

				printf(" %d", lcores[i]);
			}
			printf("\n");
		}
	}

	// quit
	for (i=0; i<ncores; i++)
		while (params[i].filled) usleep(1000);
	quit_signal = true;

	/* clean up the EAL */
	for (i=0; i<NR_FPGA; i++) {
		munmap(vec[i], sb[i].st_size);
		rte_free(dest[i]);
	}
	rte_eal_cleanup();

	return 0;
}
