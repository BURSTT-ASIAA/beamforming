#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define nvec 2

int asmfunc(float *mat, char *vec, long n, float *dest);

int main(void) {
    float dest[16] __attribute__ ((aligned (64)));
    float mat[16*16*2];
    char *vec;
    int i, j, n, p, p2;
    clock_t start, end;
    double cpu_time_used;

    for (j=0; j<16; j++) {
        for (i=0; i<16; i++) {
            p = i * 32 + j;
            p2 = i * 32 + j + 16;
            if (i == j) {
                mat[p] = 1;
                mat[p2] = 1;
            } else {
                mat[p] = 0;
                mat[p2] = 0;
            }
        }
    }

    vec = malloc(16*nvec);

    for (i=0; i<nvec; i++) {
        for (j=0; j<16; j++) {
            p = i * 16 + j;
            vec[p] = j*15;
        }
    }
    asmfunc(mat, vec, 1, dest);

    printf("Matrix:\n");
    for (j=0; j<16; j++) {
        for (i=0; i<32; i++) {
            p = j*32 + i;
            printf(" %.0f", mat[p]);
        }
        printf("\n");
    }
    printf("\nVector:");
    for (i=0; i<16; i++) {
        printf(" %hhx", vec[i]);
    }
    printf("\n");
    printf("Dest:");
    for (i=0; i<16; i++) {
        printf(" %.2f", dest[i]);
    }
    printf("\n");

    free(vec);
    return 0;
}
