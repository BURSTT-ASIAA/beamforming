#include <stdio.h>
#include <fcntl.h>
#include <mqueue.h>

typedef struct {
	int fpga;
	int index;
} queue_s;

int main(int argc, char **argv)
{
	mqd_t mqueue;
	struct mq_attr attr;
	struct {
		int fpga;
		int index;
		char padding[8];
	} data;
	int priority, i;
	ssize_t len;

//	attr.mq_flags = 0;
//	attr.mq_maxmsg = 128;
//	attr.mq_msgsize = 16;
//	attr.mq_curmsgs = 0;
//	mqueue = mq_open("/burstt", O_CREAT|O_RDWR, 0644, &attr);
	mqueue = mq_open("/burstt", O_RDWR);
	if (mqueue == (mqd_t) -1) {
		printf("Error open mqueue...\n");
		return 0;
	}

	for (i=0; i<256; i++) {
		data.fpga = i%4;
		data.index = 0;
		mq_send(mqueue, (void *)&data, sizeof(data), 0);
	}
	data.fpga = -1;
	data.index = 0;
	mq_send(mqueue, (void *)&data, sizeof(data), 0);

	mq_getattr(mqueue, &attr);
	printf("%lx, %ld, %ld, %ld\n", attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);

//	for (i=0; i<attr.mq_curmsgs; i++) {
//		len = mq_receive(mqueue, (void *)&data, sizeof(data), &priority);
//		printf("length:%d, fpga#%d, index:%d, priority:%d\n", len, data.fpga, data.index, priority);
//	}

	mq_close(mqueue);
}
