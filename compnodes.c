#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define BIGSHORT(x) (short)((((x)&255)<<8)+(((x)>>8)&255))
#define LITTLESHORT(x) ((int16_t)(x)) // Assuming data is in little-endian format

#define	NF_SUBSECTOR	0x8000

enum {BOXTOP,BOXBOTTOM,BOXLEFT,BOXRIGHT};	/* bbox coordinates */

typedef struct
{
	int16_t		x,y;
} mapvertex_t;

typedef struct
{
	int16_t		x,y,dx,dy;			/* partition line */
	int16_t		bbox[2][4];			/* bounding box for each child */
	uint16_t	children[2];		/* if NF_SUBSECTOR its a subsector */
} mapnode_t;

typedef struct
{
	int16_t		x,y,dx,dy;			/* partition line */
	uint16_t	children[2];		/* if NF_SUBSECTOR its a subsector */
	uint16_t 	encbbox[2]; 		/* encoded bounding box for each child */
} node_t;

int		    numnodes;
node_t		*nodes;

int		    numvertexes;
mapvertex_t	*vertexes;

int16_t 	worldbbox[4];

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

/*
=================
=
= P_LoadNodes
=
=================
*/


static int P_EncodeBBoxSide(int16_t *b, int16_t *outerbbox, int pc, int nc)
{
	int length, unit;
	int nu, pu;

	length = outerbbox[pc] - outerbbox[nc] + 15;
	if (length < 16)
		return 0;
	unit = length / 16;

	// negative corner is increasing
	nu = 0;
	length = b[nc] - outerbbox[nc];
	if (length > 0) {
		nu = length / unit;
		if (nu > 15)
			nu = 15;
		b[nc] = outerbbox[nc] + nu * unit;
	}

	// positive corner is decreasing
	pu = 0;
	length = outerbbox[pc] - b[pc];
	if (length > 0) {
		pu = length / unit;
		if (pu > 15)
			pu = 15;
		b[pc] = outerbbox[pc] - pu * unit;
	}

	return (pu << 4) | nu;
}

// encodes bbox as the number of 1/16th units of parent bbox on each side
static int P_EncodeBBox(int16_t *cb, int16_t *outerbbox)
{
	int encbbox;
	encbbox = P_EncodeBBoxSide(cb, outerbbox, BOXRIGHT, BOXLEFT);
	encbbox <<= 8;
	encbbox |= P_EncodeBBoxSide(cb, outerbbox, BOXTOP, BOXBOTTOM);
	return encbbox;
}

static void P_EncodeNodeBBox_r(int nn, int16_t *bboxes, int16_t *outerbbox)
{
	int 		j;
	node_t 		*n;

	if (nn & NF_SUBSECTOR)
		return;

	n = nodes + nn;
	for (j=0 ; j<2 ; j++)
	{
		int16_t *bbox = &bboxes[nn*8+j*4];
		n->encbbox[j] = P_EncodeBBox(bbox, outerbbox);
		P_EncodeNodeBBox_r(n->children[j], bboxes, bbox);
	}
}

// set the world's bounding box
// recursively encode bounding boxes for all BSP nodes
static void P_EncodeNodesBBoxes(int16_t *bboxes)
{
	P_EncodeNodeBBox_r(numnodes-1, bboxes, worldbbox);
}

void loadverts (uint8_t *data)
{
	int			i;
	mapvertex_t	*ml;
	mapvertex_t	*li;

	ml = (mapvertex_t *)data;
	li = vertexes;
	for (i=0 ; i<numvertexes ; i++, li++, ml++)
	{
		li->x = LITTLESHORT(ml->x);
		li->y = LITTLESHORT(ml->y);
	}

	worldbbox[BOXLEFT] = INT16_MAX;
	worldbbox[BOXRIGHT] = INT16_MIN;
	worldbbox[BOXBOTTOM] = INT16_MAX;
	worldbbox[BOXTOP] = INT16_MIN;

	li = vertexes;
	for (i=0 ; i<numvertexes ; i++, li++)
	{
		mapvertex_t *v = li;
		if (v->x < worldbbox[BOXLEFT])
			worldbbox[BOXLEFT] = v->x;
		if (v->x > worldbbox[BOXRIGHT])
			worldbbox[BOXRIGHT] = v->x;
		if (v->y < worldbbox[BOXBOTTOM])
			worldbbox[BOXBOTTOM] = v->y;
		if (v->y > worldbbox[BOXTOP])
			worldbbox[BOXTOP] = v->y;
	}
}

void loadnodes (uint8_t *data)
{
	int			i,j,k;
	node_t		*no;
	mapnode_t 	*mn;
	int16_t 	*bboxes;

	bboxes = malloc(numnodes * 8 * sizeof(int16_t));

	mn = (mapnode_t *)data;
	no = nodes;
	for (i=0 ; i<numnodes ; i++, no++, mn++)
	{
		no->x = LITTLESHORT(mn->x);
		no->y = LITTLESHORT(mn->y);
		no->dx = LITTLESHORT(mn->dx);
		no->dy = LITTLESHORT(mn->dy);

		for (j=0 ; j<2 ; j++)
		{
			no->children[j] = LITTLESHORT(mn->children[j]);
			for (k=0 ; k<4 ; k++)
				bboxes[i*8+j*4+k] = LITTLESHORT(mn->bbox[j][k]);
		}
	}

	P_EncodeNodesBBoxes(bboxes);

    free(bboxes);

    no = nodes;
	for (i=0 ; i<numnodes ; i++, no++)
	{
        no->x = BIGSHORT(no->x);
        no->y = BIGSHORT(no->y);
        no->dx = BIGSHORT(no->dx);
        no->dy = BIGSHORT(no->dy);

		for (j=0 ; j<2 ; j++)
		{
            no->children[j] = BIGSHORT(no->children[j]);
            no->encbbox[j] = BIGSHORT(no->encbbox[j]);
        }
    }        
}

int main(int argc, char *argv[]) {
    int length;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <nodes_file> <vertexes_file> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint8_t *data;

    data = readfile(argv[2], &length); 
    if (!data) {
        return EXIT_FAILURE;
    }

    numvertexes = length / sizeof(mapvertex_t);
	vertexes = malloc(numvertexes*sizeof(mapvertex_t));
	memset(vertexes, 0, numvertexes*sizeof(mapvertex_t));
    loadverts(data);
    free(data);
    free(vertexes);

    data = readfile(argv[1], &length); 
    if (!data) {
        return EXIT_FAILURE;
    }

    numnodes = length / sizeof(mapnode_t);
	nodes = malloc(numnodes*sizeof(node_t));
	memset(nodes, 0, numnodes*sizeof(node_t));
    loadnodes(data);
    free(data);

    FILE *outf = fopen(argv[3], "wb");
    if (!outf) {
        printf("Failed to open output file");
        free(nodes);
        return EXIT_FAILURE;
    }

    fwrite(nodes, 1, numnodes*sizeof(node_t), outf);
    fclose(outf);

    free(nodes);

    return EXIT_SUCCESS;
}