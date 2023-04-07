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
#define NR_cpu 8L // 1024 / NR_ch
#define NR_FPGA 1L
#define DATA_SIZE (8192 + 64) * 2L
#define DATA_HEADER 64L
#define NR_BUFFER 32L
#define NR_VBUFFER 5L
#define BLOCK_SIZE DATA_SIZE * NR_sum * NR_run
#define MASK_OFFSET BLOCK_SIZE * 30L
#define MASK_BLOCK_SIZE ((NR_sum * NR_run) >> 2)
#define INTENSITY_DATA_SIZE NR_run * NR_BUFFER * 1024 * 16 * 2L
#define INTENSITY_INFO_SIZE 64L
#define INTENSITY_SIZE INTENSITY_DATA_SIZE + INTENSITY_INFO_SIZE
#define VOLTAGE_BLOCK NR_sum * NR_run * NR_cpu * NR_ch
#define VOLTAGE_FILE "/dev/hugepages/voltage.bin"
#define VOLTAGE_DATA_SIZE VOLTAGE_BLOCK * NR_VBUFFER
#define VOLTAGE_INFO_SIZE 64L
#define VOLTAGE_SIZE VOLTAGE_DATA_SIZE + VOLTAGE_INFO_SIZE

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
    int cpu_id;
    int buffer_id;
} task_s;

int asmfunc(short *mat, char *vec, long nr, long nc, void *dest, char *mask, long beamid, void *voltage);

//const unsigned mem_node[4] = {0, 1, 0, 1};
const char *fpga_fn[] = {
    "/mnt/fpga0",
    "/mnt/fpga1",
    "/mnt/fpga2",
    "/mnt/fpga3"
};
const char *intensity_fn[] = {
    "/dev/hugepages/fpga0.bin",
    "/dev/hugepages/fpga1.bin",
    "/dev/hugepages/fpga2.bin",
    "/dev/hugepages/fpga3.bin",
};

_Atomic bool quit_signal = false;
_Atomic bool cpu_busy[RTE_MAX_LCORE];
_Atomic int buffer_counter[NR_FPGA * NR_BUFFER];
_Atomic bool buffer_beam[NR_FPGA * NR_BUFFER];
_Atomic int buffer_blkid[NR_FPGA * NR_BUFFER];
void *buffer[NR_FPGA], *vbuffer;
long *ip_ptr[NR_FPGA], *vp_ptr;
_Atomic int v_head=0, v_processed=0;
_Atomic int i_head[NR_FPGA], i_processed[NR_FPGA];

/* Launch a function on lcore. 8< */
static int lcore_integral(void *arg)
{
    task_s *params = arg;
    char *vec;
    void *dest, *vdest;
    long i;
    long nr, nc, beamid;
	float time_used;
	struct timeval start_t, end_t;

    while (!quit_signal) {
        if (!cpu_busy[params->cpu_id]) {
            usleep(1000);
            continue;
        }

		gettimeofday(&start_t, NULL);
//        printf("Enter integral function: CPU#%d\n", params->cpu_id);
        vec = params->vec;
        dest = params->dest;
        vdest = params->vdest;
        nr = params->nr;
        nc = params->nc;
        beamid = params->beamid;
        for (i=0; i<params->repeat; i++) {
            asmfunc(params->mat, vec, nr, nc, dest, params->mask, beamid, vdest);
            vec += DATA_SIZE * params->nr;
            dest += 1024 * 16 * 2;
            vdest += 1024;
        }
        cpu_busy[params->cpu_id] = false;
        buffer_counter[params->buffer_id]++;
		gettimeofday(&end_t, NULL);
		time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
		printf("lcores #%d(%d),  Real time: %3f\n", rte_lcore_id(), params->cpu_id, time_used);
//        printf("Exit integral function: CPU#%d  Real time: %3f  counter:%d\n", params->cpu_id, time_used, buffer_counter[params->buffer_id]);
    }

    return 0;
}
/* >8 End of launching function on lcore. */

static int lcore_socket(void *arg)
{
    int i, p;

    while (!quit_signal) {
        for (i = 0; i < NR_FPGA; i++) {
            p = i * NR_BUFFER + i_processed[i];
            if (buffer_counter[p] >= NR_cpu) {
                ip_ptr[i][i_processed[i] + 1] = (long)buffer_blkid[p];
                i_processed[i] = (i_processed[i] + 1) % NR_BUFFER;
                *ip_ptr[i] = i_processed[i];
                buffer_counter[p] = -1;
                printf("=== Processed: fpga#%d  head:%d  processed:%d ===\n", i, i_head[i], i_processed[i]);

                if (buffer_beam[p]) {
                    v_processed = (v_processed + 1) % NR_VBUFFER;
                    *vp_ptr = v_processed;
                }
            }
        }
        usleep(1000);
    }
}

/* find a feee lcore or wait */
static int find_lcore(task_s *tasks, int ncores)
{
    int i;

    while (true) {
        for (i=0; i<ncores; i++)
            if (!cpu_busy[i]) return i;
        usleep(1000);
    }
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int main(int argc, char **argv)
{
    int fd, ret;
    unsigned lcore_id;
    char *vec[NR_FPGA], *mask[NR_FPGA];
    short mat[16*16*2*1024];
    int i, j, k, p, ncores;
    task_s params[RTE_MAX_LCORE];
    struct stat sb[NR_FPGA];
    unsigned lcores[RTE_MAX_LCORE];
    long offset, offset0, length;
    mqd_t mqueue;
    struct mq_attr attr;
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
    char *mask_ptr;

    mqueue = mq_open("/burstt", O_RDONLY);
    if (mqueue == (mqd_t) -1) {
        printf("Error open mqueue...\n");
        return 0;
    }

    /* clean up mqueue fisrt */
	mq_getattr(mqueue, &attr);
	printf("mqueue: %lx, %ld, %ld, %ld\n", attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);

	for (i=0; i<attr.mq_curmsgs; i++) {
		len = mq_receive(mqueue, (void *)&mq_data, sizeof(mq_data), &priority);
		printf("[%d] length:%d, fpga#%d, index:%d, beamid:%d, priority:%d\n",
				i, len, mq_data.fpga, mq_data.index, mq_data.beamid, priority);
	}

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");

    /* Retrieve the data buffer */
    length = NR_run * NR_cpu * NR_ch * 16;
    for (i = 0; i < NR_FPGA; i++) {
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

        fd = open(intensity_fn[i], O_RDWR);
        if (fd < 0)
            rte_exit(EXIT_FAILURE, "Cannot open intensity file: %s\n", intensity_fn[i]);

        buffer[i] = mmap(NULL, INTENSITY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (buffer[i] == MAP_FAILED)
            rte_exit(EXIT_FAILURE, "Cannot mmap intensity buffer\n");
        ip_ptr[i] = (long *)(buffer[i] + INTENSITY_DATA_SIZE);

        i_head[i] = 0;
        i_processed[i] = 0;
        *ip_ptr[i] = 0;
    }

    fd = open(VOLTAGE_FILE, O_RDWR);
    if(fd < 0)
        rte_exit(EXIT_FAILURE, "Open voltage buffer failed\n");

    vbuffer = mmap(NULL, VOLTAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if(vbuffer == MAP_FAILED)
        rte_exit(EXIT_FAILURE, "Mapping voltage buffer failed\n");
    vp_ptr = (long *)(vbuffer + VOLTAGE_DATA_SIZE);
    *vp_ptr = 0;

    for (i = 0; i < NR_FPGA * NR_BUFFER; i++) {
        buffer_counter[i] = -1;
        buffer_beam[i] = false;
        buffer_blkid[i] = -1;
    }

    // transposed matrix
    for (k=0; k<1024; k++)
        for (j=0; j<16; j++)
            for (i=0; i<16; i++) {
                // p: real, p2:imag
                p = ((k * 16 + j) * 16 + i) * 2;

                if (i == j) {
                    mat[p] = 4096;
                    mat[p + 1] = 0;
                } else {
                    mat[p] = 0;
                    mat[p + 1] = 0;
                }
/*
                phase = M_PI * 2 / 16 * i * j;
                mat[p] = 256 * cosf(phase);
                mat[p + 1] = -256 * sinf(phase);
*/
            }

    /* Launches the socket+integral functions on lcores. 8< */
    i = -1;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (i < 0) {
            // socket function
            rte_eal_remote_launch(lcore_socket, NULL, lcore_id);
        } else {
            // integral funtion
            cpu_busy[i] = false;
            params[i].cpu_id = i;
            rte_eal_remote_launch(lcore_integral, &params[i], lcore_id);
            lcores[i] = lcore_id;
        }
        i++;
    }
    ncores = i;

    while(true) {
        len = mq_receive(mqueue, (void *)&mq_data, sizeof(mq_data), &priority);
//        if (len < 0) {
//            usleep(1000);
//            continue;
//        }

        printf("Data in-> length:%d, fpga#%d, index:%d, beamid:%d, priority:%d\n",
                len, mq_data.fpga, mq_data.index, mq_data.beamid, priority);
        k = mq_data.fpga;
        offset0 = mq_data.index * BLOCK_SIZE;
        beamid = mq_data.beamid;
        if (k < 0) {
            printf("Quit the process.....\n");
            break;
        }
        if (k >= NR_FPGA)
            continue;

        p = k * NR_BUFFER + i_head[k];
        if (buffer_counter[p] >= 0) {
            printf("Intensity buffer is not empty: fpga%d[%d]\n", k, i_head[k]);
        }

        mask_ptr = mask[k] + mq_data.index * MASK_BLOCK_SIZE;
        printf("mask value:");
        for (j=0; j<32; j++)
            printf(" %02hhX", mask_ptr[j]);
        printf("\n");
        printf("Processing fpga#%d  head:%d  processed:%d  cpus:", k, i_head[k], i_processed[k]);

        buffer_counter[p] = 0;
        buffer_blkid[p] = mq_data.index;
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
            params[i].dest = buffer[k] + (j * NR_ch * 16 + i_head[k] * length) * 2;
            params[i].beamid = beamid;
            params[i].vdest = vbuffer + v_head * VOLTAGE_BLOCK + j * NR_ch;
            params[i].mask = mask_ptr + (j * NR_ch / 512);
            params[i].buffer_id = p;

            cpu_busy[i] = true;
            printf(" %d", lcores[i]);
        }
        printf("\n");

        i_head[k]++;
        if (i_head[k] >= NR_BUFFER) i_head[k] = 0;

        if (beamid >= 0) {
            buffer_beam[p] = true;
            v_head++;
            if (v_head >= NR_VBUFFER) v_head = 0;
            if (v_head == v_processed)
                printf("Voltage data writing too slow\n");
        } else {
            buffer_beam[p] = false;
        }
    }

    // quit
    for (i=0; i<ncores; i++)
        while (cpu_busy[i]) usleep(1000);
    quit_signal = true;

    /* clean up the EAL */
    for (i=0; i<NR_FPGA; i++) {
        munmap(vec[i], sb[i].st_size);
        munmap(buffer[i], INTENSITY_SIZE);
    }
    munmap(vbuffer, VOLTAGE_SIZE);
    rte_eal_cleanup();

    mq_close(mqueue);
    return 0;
}
