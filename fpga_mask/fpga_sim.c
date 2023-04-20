#include <stdio.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define DATA_SIZE (8192 + 64) * 2L
#define DATA_HEADER 64L
#define FPGA_FILE "/mnt/fpga0"
#define DATA_PER_BLOCK 400000L
#define FPGA_BLOCK_SIZE DATA_SIZE * DATA_PER_BLOCK
#define FPGA_NBLOCKS 20L
#define FPGA_DATA_SIZE FPGA_BLOCK_SIZE * FPGA_NBLOCKS
#define MASK_SIZE (DATA_PER_BLOCK >> 2) * FPGA_NBLOCKS

static bool keepRunning = true;
static ushort header[32] = {
	0xb0f3, 0xaeef, 0xcc8c, 0xaabb, 0x0000, 0x0000, 0x0300, 0x0800,
	0x0201, 0x0201, 0x0201, 0x0201, 0xe78e, 0xdf61, 0x195d, 0x0001,
	0xe58b, 0xdf61, 0x545d, 0x4d49, 0x101e, 0x0a11, 0x5049, 0x5049,
	0xcd8d, 0x000b, 0x5050, 0x4253, 0x5542, 0x5352, 0x5454, 0x5454
};

void intHandler(int sig) {
	keepRunning = false;
}

int main(int argc, char **argv)
{
	mqd_t mqueue;
	struct mq_attr attr;
	struct {
		int fpga;
		int index;
		int beamid;
		char padding[4];
	} data;
	int priority, i;
	ssize_t len;
	struct sigaction act;
	int fd;
	char *vec, *mask, *ptr;
	struct stat sb;
	unsigned long counter = 0;

	act.sa_handler = intHandler;
	sigaction(SIGINT, &act, NULL);

	attr.mq_flags = 0;
	attr.mq_maxmsg = 128;
	attr.mq_msgsize = sizeof(data);
	attr.mq_curmsgs = 0;
	mqueue = mq_open("/burstt", O_CREAT|O_RDWR, 0666, &attr);
	if (mqueue == (mqd_t) -1) {
		printf("Error open mqueue...\n");
		return 0;
	}

	fd = open(FPGA_FILE, O_RDWR);
	if (fd < 0) {
		printf("Cannot open fpga file: %s\n", FPGA_FILE);
		return 0;
	}

	if (fstat(fd, &sb) < 0) {
		printf("Cannot fstat fpga file: %s\n", FPGA_FILE);
		return 0;
	}

	vec = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (vec == MAP_FAILED) {
		printf("Cannot mmap data buffer\n");
		return 0;
	}
	mask = vec + FPGA_DATA_SIZE;

	for (i = 0; i < FPGA_NBLOCKS; i++) {
		ptr = vec + FPGA_BLOCK_SIZE * i;
		memcpy(ptr, header, DATA_HEADER);
	}

	for (i= 0; i < MASK_SIZE; i++)
		mask[i] = 0xfc;

	i = 0;
	data.fpga = 0;
	data.beamid = -1;
	while (keepRunning) {
		ptr = vec + FPGA_BLOCK_SIZE * i;
		memcpy(ptr, &counter, 5);
		counter += DATA_PER_BLOCK * 2;

		data.index = i;
		mq_send(mqueue, (void *)&data, sizeof(data), 0);
		i = (i + 1) % FPGA_NBLOCKS;
		printf("Send mqueue: fpga#%d, index:%d, beamid:%d\n",
				data.fpga, data.index, data.beamid);
		usleep(1000000);
	}
//	data.fpga = -1;
//	data.index = 0;
//	data.beamid = -1;
//	mq_send(mqueue, (void *)&data, sizeof(data), 0);

	mq_getattr(mqueue, &attr);
	printf("Exit: %lx, %ld, %ld, %ld\n", attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);

	for (i=0; i<attr.mq_curmsgs; i++) {
		len = mq_receive(mqueue, (void *)&data, sizeof(data), &priority);
		printf("[%d] length:%d, fpga#%d, index:%d, beamid:%d, priority:%d\n",
				i, len, data.fpga, data.index, data.beamid, priority);
	}

	munmap(vec, sb.st_size);
	mq_close(mqueue);
}
