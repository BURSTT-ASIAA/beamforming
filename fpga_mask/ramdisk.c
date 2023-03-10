#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
//#include <x86intrin.h>

#define RAMDISK "/bonsai/beams.bin"
//#define RAMDISK "beams.bin"
#define RAMDISK_LENGTH 1024*16*4*1000*60L
#define BLOCK_LENGTH 1024*16*4

int main(int argc, char **argv)
{
	int fd ,ret;
	void *buffer, *ptr;
	int j, k;
	float __attribute__ ((aligned (64))) input[1024][16];
	float *fptr;

	fd = open(RAMDISK, O_RDWR);
	buffer = mmap(NULL, RAMDISK_LENGTH, PROT_WRITE, MAP_SHARED, fd, 0);
	if(buffer == MAP_FAILED){
		printf("Mapping Failed\n");
		return 1;
	}
	close(fd);

	for (j=0; j<1024; j++) {
		for (k=0; k<16; k++) {
			input[j][k] = j + k;
		}
	}
	fptr = input[0];
	printf("Input value: %f %f %f %f\n", fptr[0], fptr[1], fptr[2], fptr[3]);

	ptr = buffer;
	for (j=0; j<4; j++) {
		memcpy(ptr, input, BLOCK_LENGTH);
		msync(ptr, BLOCK_LENGTH, MS_SYNC);
		ptr += BLOCK_LENGTH;
	}
	fptr = buffer;
	printf("Output value: %f %f %f %f\n", fptr[0], fptr[1], fptr[2], fptr[3]);

	munmap(buffer, RAMDISK_LENGTH);
}
