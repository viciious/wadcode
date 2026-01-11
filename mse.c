#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define BIGSHORT(x) (short)((((x)&255)<<8)+(((x)>>8)&255))
#define LITTLESHORT(x) ((int16_t)(x)) // Assuming data is in little-endian format

typedef struct
{
        int16_t         numsegs;
        int16_t         firstseg;                       /* segs are stored sequentially */
} mapsubsector_t;

typedef struct
{
        int16_t         numsegs;
} subsector_t;

uint8_t *readfile(const char *filename, int *flength) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *buffer = malloc(length + 1);
    if (!buffer) {
        fclose(file);
        printf("Failed to allocate memory");
        return NULL;
    }

    *flength = length;
    fread(buffer, 1, length, file);
    buffer[length] = '\0'; // Null-terminate the string

    fclose(file);
    return buffer;
}

int main (int argc, char **argv) {
	int i, l1, l2, l;
	double mse;

	uint8_t *b1 = readfile(argv[1], &l1);
	uint8_t *b2 = readfile(argv[2], &l2);

	l = l1;
	if (l2 < l1) l = l2;

	mse = 0;
	for (i = 0; i < l; i++) {
		mse += (b1[i] - b2[i])*(b1[i] - b2[i]);
	}
	mse /= (double)l;

	printf("%s\t%s\t%d\t%.3f\n", argv[1], argv[2], l, mse);
	return 0;
}

