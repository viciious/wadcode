#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define BIGSHORT(x) (short)((((x)&255)<<8)+(((x)>>8)&255))
#define LITTLESHORT(x) ((int16_t)(x)) // Assuming data is in little-endian format

typedef struct
{
	int16_t		numsegs;
	int16_t		firstseg;			/* segs are stored sequentially */
} mapsubsector_t;

typedef struct
{
	int16_t		numsegs;
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

void loadsubsectors (uint8_t *data, subsector_t *subsectors, int numsubsectors)
{
	int				i;
	int 			count;
	mapsubsector_t	*ms;
	subsector_t		*ss;

	ms = (mapsubsector_t *)data;
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
	{
		ss->numsegs = LITTLESHORT(ms->numsegs);
	}

	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++)
	{
		ss->numsegs = BIGSHORT(ss->numsegs);
	}
}

int main(int argc, char *argv[]) {
    int length;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint8_t *data = readfile(argv[1], &length); 
    if (!data) {
        return EXIT_FAILURE;
    }

    int numssec = length / sizeof(mapsubsector_t);
	subsector_t *ssec = malloc(numssec*sizeof(subsector_t));
	memset(ssec, 0, numssec*sizeof(subsector_t));
    loadsubsectors(data, ssec, numssec);
    free(data);

    FILE *outf = fopen(argv[2], "wb");
    if (!outf) {
        printf("Failed to open output file");
        free(ssec);
        return EXIT_FAILURE;
    }

    fwrite(ssec, 1, sizeof(subsector_t)*numssec, outf);
    fclose(outf);

    free(ssec);

    return EXIT_SUCCESS;
}