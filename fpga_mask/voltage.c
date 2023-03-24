#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define SRC_FILE "/dev/hugepages/voltage.bin"
#define DATA_SIZE 1024*1000*400*5L
#define INFO_SIZE 64L
#define FILE_SIZE DATA_SIZE + INFO_SIZE
#define OUTPUT_PREFIX "/sdisk1/VoltageData/T"
#define MAX_FILE_SIZE 1024*1000*400*60L

static bool keepRunning = true;

void intHandler(int sig) {
    keepRunning = false;
}

/* generate a filename from current time */
void voltage_fname(char *cbuf)
{
    sprintf(cbuf, OUTPUT_PREFIX"%010ld", time(NULL));
}

void write_data(void *buf, long length)
{
    char fname[80];
    void *ptr;
    static int vfd = -1;
    static long fill_count = 0;
    long left, transfered;

    if (vfd < 0) {
        voltage_fname(fname);
        vfd = open(fname, O_CREAT|O_WRONLY|O_TRUNC, 0644);
        if(vfd < 0) {
            printf("Error: Open voltage file failed\n");
            return;
        }
        fill_count = 0;
    }

    if (fill_count + length > MAX_FILE_SIZE) {
        printf("Warning: something wrong.....\n");
    }

    left = length;
    ptr = buf;
    while (left > 0) {
        transfered = write(vfd, ptr, left);
        if (transfered < 0) {
            printf("Disk writing for voltage data failed\n");
            close(vfd);
            vfd = -1;
            fill_count = 0;
            return;
        } else {
            ptr += transfered;
            left -= transfered;
        }
    }

    fill_count += length;
    if (fill_count >= MAX_FILE_SIZE) {
        close(vfd);
        vfd = -1;
        fill_count = 0;
    }

    return;
}

int main(int argc, char **argv)
{
    int fd;
    void *buf, *ptr;
    long *head;
    long _head, _head2, length;
    struct sigaction act;

    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, NULL);

    fd = open(SRC_FILE, O_RDWR);
    if (fd > 0) {
        buf = mmap(NULL, FILE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
        if (buf == MAP_FAILED) {
            printf("Source Memory Mapping Failed\n");
            return 1;
        }
        head = (long *)(buf + DATA_SIZE);
        close(fd);
    } else {
        printf("Source File Open Failed\n");
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
            write_data(buf + _head, length);
            _head = 0;
        }

        if (_head < _head2) {
            // write from _head to _head2
            length = _head2 - _head;
            write_data(buf + _head, length);
            _head = _head2;
        }

        usleep(1000);
    };

    munmap(buf, FILE_SIZE);
    printf("\nExit.......\n");
    return 0;
}
