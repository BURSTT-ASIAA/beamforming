#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
//#include <x86intrin.h>

#define RAMDISK "/bonsai/beams.bin"
#define RAMDISK_LENGTH 1024*16*2*4000*60L
#define BLOCK_LENGTH 1024*16*2

void f32tof16(void *, void *);

int main(int argc, char **argv)
{
	int fd ,ret;
	char *buffer, *ptr;
	int j, k;
	float __attribute__ ((aligned (64))) input[1024][16];
	char __attribute__ ((aligned (64))) output[BLOCK_LENGTH];

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
	f32tof16(output, input);

	ptr = buffer;
	for (j=0; j<4; j++) {
		memcpy(ptr, output, BLOCK_LENGTH);
		msync(ptr, BLOCK_LENGTH, MS_SYNC);
		ptr += BLOCK_LENGTH;
	}

	printf("Value: %hhu %hhu %hhu %hhu\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	munmap(buffer, RAMDISK_LENGTH);
}
