#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define BIGSHORT(x) (short)((((x)&255)<<8)+(((x)>>8)&255))
#define LITTLESHORT(x) ((int16_t)(x)) // Assuming data is in little-endian format

// truncate offset to 14 bits along the way
#define SEG_PACK(seg,side) do { seg_t *s = (seg); s->linedef <<= 1; s->linedef |= (!!(side)); } while (0)

typedef struct seg_s
{
	int16_t	linedef;
	int16_t	v1, v2;
} seg_t;

typedef struct
{
	int16_t		v1, v2;
	int16_t		angle;
	int16_t		linedef, side;
	int16_t		offset;
} mapseg_t;

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

void loadsegs (uint8_t *data, seg_t *segs, int numsegs)
{
	int			i;
	mapseg_t	*ml;
    seg_t      *li;

	ml = (mapseg_t *)data;
	li = segs;
	for (i=0 ; i<numsegs ; i++, li++, ml++)
	{
        int linedef, side;

        li->v1 = (unsigned)LITTLESHORT(ml->v1);
		li->v2 = (unsigned)LITTLESHORT(ml->v2);

		linedef = (unsigned)LITTLESHORT(ml->linedef);

		li->linedef = linedef;

		side = (unsigned)LITTLESHORT(ml->side);

        side = !!side;

        SEG_PACK(li, side);

        li->v1 = BIGSHORT(li->v1);
        li->v2 = BIGSHORT(li->v2);
        li->linedef = BIGSHORT(li->linedef);
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

    int numsegs = length / sizeof(mapseg_t);
	seg_t *segs = malloc(numsegs*sizeof(seg_t));
	memset(segs, 0, numsegs*sizeof(seg_t));

    loadsegs(data, segs, numsegs);

    FILE *outf = fopen(argv[2], "wb");
    if (!outf) {
        printf("Failed to open output file");
        free(data);
        free(segs);
        return EXIT_FAILURE;
    }

    fwrite(segs, 1, sizeof(seg_t)*numsegs, outf);
    fclose(outf);

    free(data);
    free(segs);

    return EXIT_SUCCESS;
}