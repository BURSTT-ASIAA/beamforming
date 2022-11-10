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
#include <mqueue.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_malloc.h>

//#define NR_pack 400*1000UL // 6.4G
#define NR_sum 500UL  // integral count
#define NR_run 1000UL // NR_pack / NR_sum
#define NR_ch 128UL // must be multiple of 16
#define NR_cpu 8 // 1024 / NR_ch
#define NR_FPGA 4
#define DATA_SIZE (8192 + 64) * 2
#define DATA_HEADER 64
#define NR_BUFFER 4
#define BLOCK_SIZE DATA_SIZE * NR_sum * NR_run

typedef struct __attribute__ ((aligned (64))) {
	short *mat;
	char *vec;
	long nr;
	long nc;
	long repeat;
	float *dest;
	bool filled;
	bool notify;
} task_s;
typedef struct __attribute__ ((aligned (64))) {
	bool *notify_p[NR_cpu];
	void *data_p;
	long length;
	bool filled;
} status_s;
typedef struct {
	void *data_p;
	long length;
	bool *filled_p;
} ring_s;

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
static int lcore_integral(void *arg)
{
	task_s *params = arg;
	char *vec;
	float *dest;
	long i;
//	float time_used;
//	struct timeval start_t, end_t;

	while (!quit_signal) {
		if (!params->filled) {
			usleep(1000);
			continue;
		}

//		gettimeofday(&start_t, NULL);
		vec = params->vec;
		dest = params->dest;
		for (i=0; i<params->repeat; i++) {
			asmfunc(params->mat, vec, params->nr, params->nc, dest);
			vec += DATA_SIZE * params->nr;
			dest += 1024 * 16;
		}
//		(*params->counter)++;
		params->notify = true;
		params->filled = false;
//		gettimeofday(&end_t, NULL);
//		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
//		printf("lcores #%d,  Real time: %3f\n", rte_lcore_id(), time_used);
	}

	return 0;
}
/* >8 End of launching function on lcore. */

static int lcore_socket(void *arg)
{
	status_s *status_p;
	int counter[NR_FPGA*NR_BUFFER];
	int mask_arr[NR_FPGA*NR_BUFFER], mask;
	int i, j;
	ring_s rings[NR_FPGA*NR_BUFFER], *ring_p;
	int ring_head, ring_tail;

	for (i=0; i<NR_FPGA*NR_BUFFER; i++) {
		counter[i] = 0;
		mask_arr[i] = 0;
	}
	ring_head = 0;
	ring_tail = 0;

	while (!quit_signal) {
		status_p = arg;
		for (i=0; i<NR_FPGA*NR_BUFFER; i++, status_p++) {
			if (!status_p->filled) continue;

//			printf("get notify fpga%d[%d], pointer:%p\n",
//					i/NR_BUFFER, i%NR_BUFFER, status_p);
			mask = 1;
			for (j=0; j<NR_cpu; j++, mask<<=1) {
				if (mask_arr[i] & mask) continue;

				if (*status_p->notify_p[j]) {
					mask_arr[i] |= mask;
					*status_p->notify_p[j] = false;
					counter[i]++;
				}
			}

			if (counter[i] >= NR_cpu) {
				mask_arr[i] = 0;
				counter[i] = 0;
				rings[ring_head].data_p = status_p->data_p;
				rings[ring_head].length = status_p->length;
				rings[ring_head].filled_p = &status_p->filled;
				ring_head++;
				if (ring_head >= NR_FPGA*NR_BUFFER) ring_head = 0;
			}
		}

		if (ring_head != ring_tail) {
			ring_p = &rings[ring_tail];
//			printf("Buffer(%p) filled, sending packets(%lx)...\n", ring_p->data_p, ring_p->length);
			*ring_p->filled_p = false;
			ring_tail++;
			if (ring_tail >= NR_FPGA*NR_BUFFER) ring_tail = 0;
			usleep(100);
		} else {
			usleep(1000);
		}
	}
}

/* find a feee lcore or wait */
static int find_lcore(task_s *tasks, int ncores)
{
	int i;

	while (true) {
		for (i=0; i<ncores; i++)
			if (!tasks[i].filled && !tasks[i].notify) return i;
		usleep(1000);
	}
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int main(int argc, char **argv)
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
	long offset, offset0, length;
	int b_index[NR_FPGA];
	status_s status[NR_FPGA*NR_BUFFER];
	status_s *status_p;
	mqd_t mqueue;
	struct {
		int fpga;
		int index;
		char padding[8];
	} mq_data;
	int priority;
	ssize_t len;

	mqueue = mq_open("/burstt", O_RDONLY);
	if (mqueue == (mqd_t) -1) {
		printf("Error open mqueue...\n");
		return 0;
	}

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	/* Retrieve the data buffer */
	length = NR_run * NR_cpu * NR_ch * 16;
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

		dest[i] = rte_malloc_socket(NULL, 4 * length * NR_BUFFER, 0x40, mem_node[i]);
		if (dest[i] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init integral buffer\n");
		b_index[i] = 0;
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

	// init status_s
	status_p = status;
	for (i=0; i<NR_FPGA; i++)
		for (j=0; j<NR_BUFFER; j++) {
			status_p->filled = false;
			status_p->data_p = (void *)dest[i] + 4 * j * length;
			status_p->length = length * 4;
			status_p++;
		}

	/* Launches the socket+integral functions on lcores. 8< */
	i = -1;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (i < 0) {
			// socketfunction
			rte_eal_remote_launch(lcore_socket, status, lcore_id);
		} else {
			// integral funtion
			params[i].filled = false;
			rte_eal_remote_launch(lcore_integral, &params[i], lcore_id);
			lcores[i] = lcore_id;
		}
		i++;
	}
	ncores = i;

	while(true) {
		len = mq_receive(mqueue, (void *)&mq_data, sizeof(mq_data), &priority);
		printf("Data in-> length:%d, fpga#%d, index:%d, priority:%d\n",
				len, mq_data.fpga, mq_data.index, priority);
		k = mq_data.fpga;
		offset0 = mq_data.index * BLOCK_SIZE;
		if (k < 0) {
			printf("Quit the process.....\n");
			break;
		}

		status_p = status + k * NR_BUFFER + b_index[k];
		if (status_p->filled) {
			printf("output buffer is not empty: fpga%d[%d]\n", k, b_index[k]);
		}

		printf("Processing fpga#%d  cpus:", k);
		for (j=0; j<NR_cpu; j++) {
			i = find_lcore(params, ncores);
			if (j * NR_ch < 512)
				offset = offset0 + DATA_HEADER;
			else
				offset = offset0 + DATA_HEADER * 2;
			params[i].mat = mat + j * NR_ch * 512;
			params[i].vec = vec[k] + j * NR_ch * 16 + offset;
			params[i].nr = NR_sum;
			if ((j+1) * NR_ch <= 1024)
				params[i].nc = NR_ch;
			else
				params[i].nc = 1024 - j * NR_ch;
			params[i].repeat = NR_run;
			params[i].dest = dest[k] + j * NR_ch * 16 + b_index[k] * length;
			params[i].filled = true;
			status_p->notify_p[j] = &params[i].notify;

			printf(" %d", lcores[i]);
		}
		printf("\n");

		status_p->filled = true;
		b_index[k]++;
		if (b_index[k] >= NR_BUFFER) b_index[k] = 0;
	}

	// quit
	for (i=0; i<ncores; i++)
		while (params[i].filled) usleep(1000);
	status_p = status;
	for (i=0; i<NR_FPGA*NR_BUFFER; i++) {
		while (status_p->filled) usleep(1000);
		status_p++;
	}
	quit_signal = true;

	/* clean up the EAL */
	for (i=0; i<NR_FPGA; i++) {
		munmap(vec[i], sb[i].st_size);
		rte_free(dest[i]);
	}
	rte_eal_cleanup();

	mq_close(mqueue);
	return 0;
}
