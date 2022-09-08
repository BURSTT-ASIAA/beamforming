#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define nvec 40000000

int asmfunc(short *mat, char *vec, long n, float *dest);

int main(void) {
    short mat[16*16*2];
    float dest[16] __attribute__ ((aligned (64)));
    char *vec;
    int i, j, n, p, p2;
    clock_t start, end;
    struct timeval start_t, end_t;
    double cpu_time_used, time_used;

    for (j=0; j<16; j++) {
        for (i=0; i<16; i++) {
            p = i * 32 + j * 2;
            p2 = i * 32 + j * 2 + 1;
            if (i == j) {
                mat[p] = 1;
                mat[p2] = 1;
            } else {
                mat[p] = 0;
                mat[p2] = 0;
            }
        }
    }

    /* random values (or 0?) */
    vec = malloc(16*nvec);

    for (i=0; i<10; i++) {
        start = clock();
        gettimeofday(&start_t, NULL);
        asmfunc(mat, vec, nvec, dest);
        end = clock();
        gettimeofday(&end_t, NULL);
        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
        time_used = (end_t.tv_sec - start_t.tv_sec) + (end_t.tv_usec - start_t.tv_usec) / 1000000.;
        printf("Datasize: %d x 16 bytes  CPU time : %.3f  Real time : %.3f\n", nvec, cpu_time_used, time_used);
    }

    free(vec);
    return 0;
}
