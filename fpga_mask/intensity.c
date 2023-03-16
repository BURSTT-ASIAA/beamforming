#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>

#define SRC_FILE "/dev/hugepages/beams.bin"
#define DATA_SIZE 1024*16*4*1000*60L
#define INFO_SIZE 64L
#define FILE_SIZE DATA_SIZE + INFO_SIZE
#define DEST_FILE "/bonsai/beams.bin"

static bool keepRunning = true;

void intHandler(int sig) {
    keepRunning = false;
}

int main(int argc, char **argv)
{
    int fd;
    void *src_buf, *dest_buf, *src_ptr, *dest_ptr;
    long *head;
    long _head, _head2, length;
    struct sigaction act;

    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, NULL);

    fd = open(SRC_FILE, O_RDWR);
    if (fd > 0) {
        src_buf = mmap(NULL, FILE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
        if(src_buf == MAP_FAILED){
            printf("Source Memory Mapping Failed\n");
            return 1;
        }
        head = (long *)(src_buf + DATA_SIZE);
        close(fd);
    } else {
        printf("Source File Open Failed\n");
        return 1;
    }

    fd = open(DEST_FILE, O_RDWR);
    if (fd > 0) {
        dest_buf = mmap(NULL, DATA_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
        if(dest_buf == MAP_FAILED){
            printf("Destination Memory Mapping Failed\n");
            return 1;
        }
        close(fd);
    } else {
        printf("Detination File Open Failed\n");
        return 1;
    }

    _head = *head;
    printf("Head value: %d\n", _head);
    printf("Ready to transfer data.......\n");

    while (keepRunning) {
        _head2 = *head;
        if (_head > _head2) {
            // write until the end of the file
            length = DATA_SIZE - _head;
            memcpy(dest_buf + _head, src_buf + _head, length);
            msync(dest_buf + _head, length, MS_SYNC);

            _head = 0;
        }

        if (_head < _head2) {
            // write from _head to _head2
            length = _head2 - _head;
            memcpy(dest_buf + _head, src_buf + _head, length);
            msync(dest_buf + _head, length, MS_SYNC);

            _head = _head2;
        }

        usleep(1000);
    };

    munmap(src_buf, FILE_SIZE);
    munmap(dest_buf, DATA_SIZE);
    printf("\nExit.......\n");
    return 0;
}
