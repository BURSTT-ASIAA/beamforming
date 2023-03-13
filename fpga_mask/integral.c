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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <math.h>

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
#define DATA_SIZE (8192 + 64) * 2L
#define DATA_HEADER 64
#define NR_BUFFER 4
#define BLOCK_SIZE DATA_SIZE * NR_sum * NR_run
#define MASK_OFFSET BLOCK_SIZE * 16L
#define RAMDISK "/bonsai/beams.bin"
#define RAMDISK_SIZE 1024*16*4*1000*60L
#define VOLTAGE_BLOCK NR_sum * NR_run * NR_cpu * NR_ch
#define VOLTAGE_PREFIX "/disk1/voltage/T"
#define VOLTAGE_PER_FILE 60

typedef struct __attribute__ ((aligned (64))) {
	short *mat;
	char *vec;
	int nr;
	int nc;
	int repeat;
	int beamid;
	void *dest;
	void *vdest;
	char *mask;
	bool filled;
	bool notify;
} task_s;
typedef struct __attribute__ ((aligned (64))) {
	bool *notify_p[NR_cpu];
	void *data_p;
	long length;
	int beamid;
	bool filled;
} status_s;
typedef struct {
	void *data_p;
	long length;
	bool *filled_p;
} ring_s;

int asmfunc(short *mat, char *vec, long nr, long nc, void *dest, char *mask, long beamid, void *voltage);

const unsigned mem_node[4] = {0, 1, 2, 3};
const char *fpga_fn[] = {
	"/mnt/fpga0/fpga.bin",
	"/mnt/fpga1/fpga.bin",
	"/mnt/fpga2/fpga.bin",
	"/mnt/fpga3/fpga.bin"
};
volatile int quit_signal = false;
void *vbuffer;
volatile int v_head=0, v_tail=0;

/* generate a filename from current time */
void voltage_name(char *cbuf)
{
	sprintf(cbuf, VOLTAGE_PREFIX"%010ld", time(NULL));
}

/* Launch a function on lcore. 8< */
static int lcore_integral(void *arg)
{
	task_s *params = arg;
	char *vec;
	void *dest, *vdest;
	long i;
	long nr, nc, beamid;
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
		vdest = params->vdest;
		nr = params->nr;
		nc = params->nc;
		beamid = params->beamid;
		for (i=0; i<params->repeat; i++) {
			asmfunc(params->mat, vec, nr, nc, dest, params->mask, beamid, vdest);
			vec += DATA_SIZE * params->nr;
			dest += 1024 * 16 * 4;
			vdest += 1024;
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
	long len, offset = 0;
	char *data_p;
	ring_s rings[NR_FPGA*NR_BUFFER], *ring_p;
	int ring_head, ring_tail;
	int fd, vfd;
	char *buffer, *ptr, vname[80];
	int voltage_ready, voltage_count;
	ssize_t left, transfered;
	bool voltage_output = true;

	for (i=0; i<NR_FPGA*NR_BUFFER; i++) {
		counter[i] = 0;
		mask_arr[i] = 0;
	}
	ring_head = 0;
	ring_tail = 0;

	// open the NFS ramdisk
	fd = open(RAMDISK, O_RDWR);
	buffer = mmap(NULL, RAMDISK_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if(buffer == MAP_FAILED){
		printf("Mapping ramdisk failed\n");
	}

	// open the voltage data file
	voltage_name(vname);
	vfd = open(vname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	voltage_ready = 0;
	voltage_count = 0;

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
				if (status_p->beamid >= 0) voltage_ready++;
			}
		}

		while (ring_head != ring_tail) {
			ring_p = &rings[ring_tail];
//			printf("Buffer(%p) filled, sending packets(%lx)...\n", ring_p->data_p, ring_p->length);

			if(buffer != MAP_FAILED){
				len = ring_p->length;
				data_p = ring_p->data_p;
				ptr = buffer + offset;

				memcpy(ptr, data_p, len);
				msync(ptr, len, MS_SYNC);
				offset += len;
				if (offset + len > RAMDISK_SIZE) {
					offset = 0;
				}
			}
			*ring_p->filled_p = false;
			ring_tail++;
			if (ring_tail >= NR_FPGA*NR_BUFFER) ring_tail = 0;
		}

		while (voltage_ready > 0) {
			if (v_head == v_tail)
				printf("Voltage data buffer may be overwriting\n");

			if (voltage_output) {
				left = VOLTAGE_BLOCK;
				data_p = vbuffer + v_tail * VOLTAGE_BLOCK;
				while (left > 0) {
					transfered = write(vfd, data_p, left);
					if (transfered < 0) {
						printf("Disk writing for voltage data failed\n");
						printf("Halt the voltage output\n");
						voltage_output = false;
						break;
					} else {
						data_p += transfered;
						left -= transfered;
					}
				}

				if (voltage_output) voltage_count++;
				if (voltage_count >= VOLTAGE_PER_FILE) {
					close(vfd);
					voltage_name(vname);
					vfd = open(vname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
					voltage_count = 0;
				}
			}

			voltage_ready--;
			v_tail++;
			if (v_tail >= NR_BUFFER) v_tail = 0;
		}

		usleep(1000);
	}

	munmap(buffer, RAMDISK_SIZE);
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
	char *vec[NR_FPGA], *mask[NR_FPGA];
	short mat[16*16*2*1024];
	void *dest[NR_FPGA];
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
		int beamid;
		char padding[4];
	} mq_data;
	int priority;
	ssize_t len;
	int beamid;
	float phase;

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
		mask[i] = vec[i] + MASK_OFFSET;

		dest[i] = rte_malloc_socket(NULL, 4 * length * NR_BUFFER, 0x40, mem_node[i]);
		if (dest[i] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init integral buffer\n");
		b_index[i] = 0;
	}

	vbuffer = rte_malloc(NULL, VOLTAGE_BLOCK * NR_BUFFER, 0x40);
	if (vbuffer == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init voltage buffer\n");

	// transposed matrix
	for (k=0; k<1024; k++)
		for (j=0; j<16; j++)
			for (i=0; i<16; i++) {
				// p: real, p2:imag
				p = ((k * 16 + j) * 16 + i) * 2;
/*
				if (i == j) {
					mat[p] = 4096;
					mat[p + 1] = 0;
				} else {
					mat[p] = 0;
					mat[p + 1] = 0;
				}
*/
				phase = M_PI * 2 / 16 * i * j;
				mat[p] = 256 * cosf(phase);
				mat[p + 1] = -256 * sinf(phase);
			}

	// init status_s
	status_p = status;
	for (i=0; i<NR_FPGA; i++)
		for (j=0; j<NR_BUFFER; j++) {
			status_p->filled = false;
			status_p->data_p = dest[i] + 4 * j * length;
			status_p->length = length * 2;
			status_p->beamid = -1;
			status_p++;
		}

	/* Launches the socket+integral functions on lcores. 8< */
	i = -1;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (i < 0) {
			// socket function
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
		printf("Data in-> length:%d, fpga#%d, index:%d, beamid:%d, priority:%d\n",
				len, mq_data.fpga, mq_data.index, mq_data.beamid, priority);
		k = mq_data.fpga;
		offset0 = mq_data.index * BLOCK_SIZE;
		beamid = mq_data.beamid;
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
			params[i].dest = dest[k] + (j * NR_ch * 16 + b_index[k] * length) * 4;
			params[i].beamid = beamid;
			params[i].vdest = vbuffer + v_head * VOLTAGE_BLOCK + j * NR_ch;
			params[i].mask = mask[k] + (j * NR_ch / 512);
			params[i].filled = true;
			status_p->notify_p[j] = &params[i].notify;

			printf(" %d", lcores[i]);
		}
		printf("\n");

		status_p->beamid = beamid;
		if (beamid >= 0) {
			v_head++;
			if (v_head == v_tail)
				printf("Disk writing for voltage data too slow\n");
			if (v_head >= NR_BUFFER) v_head = 0;
		}

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
	rte_free(vbuffer);
	rte_eal_cleanup();

	mq_close(mqueue);
	return 0;
}
