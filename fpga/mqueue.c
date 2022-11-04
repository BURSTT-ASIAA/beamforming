#include <stdio.h>
#include <fcntl.h>
#include <mqueue.h>

int main(int argc, char **argv)
{
    mqd_t mqueue;
    struct mq_attr attr;

    mqueue = mq_open("/burstt", O_RDWR);
    if (mqueue == (mqd_t) -1) {
        printf("Error open mqueue...\n");
        return 0;
    }
    mq_getattr(mqueue, &attr);
    printf("%lx, %ld, %ld, %ld\n", attr.mq_flags, attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
    mq_close(mqueue);
}
