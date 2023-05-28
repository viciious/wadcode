#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define		VERSION			1.10
#define		WAD_FORMAT		1

// WAD_FORMAT:
// 0 -- (versions 0.99 - 1.00)
// 1 -- (current)
// 
// _32X_ lump doesn't have any info in versions prior to v1.10. When _32X_
// doesn't exist or is empty, we assume the format is 0.



// Use this define to make 32X WAD files viewable in WAD file editors
//#define		FORCE_LITTLE_ENDIAN

// Use this define to make importer create a stand-alone 32X WAD
#define		IMPORT_MODE_WAD

// Use this define when exporting a Jaguar WAD file (not finished)
//#define		WADTYPE_JAG



int label;
int lump_count;
int table_ptr;
int file_size;
int iwad_ptr;



// -- dirty fix --
// corrects lump numbers in command
// window for PNAMES and lumps after it
int lump_value_fix = 0;



int out_file_size = 0xC;
int out_table_size = 0;
int out_table_ptr = 0;	// used temporarily
int out_lump_count;

/////////////////////////////////////////
// These are used when doing PC_2_MARS //
int num_of_textures = 0;
unsigned char *out_texture1_data = 0;
unsigned char *out_texture1_table = 0;
/////////////////////////////////////////

unsigned char *data;
unsigned char *table;

unsigned char *out_table;

unsigned char *pnames_table = 0;
int patches_count;

int texture1_ptr;
int texture1_size;

unsigned char *texture1;

char first_sprite = 1;
char first_texture = 1;
char first_floor = 1;

char texture_end_written = 0;
char floor_end_written = 0;
char sprite_end_written = 0;

int d = 0;
int t = 0;

int i;
int n;

unsigned char format = 0;

#ifndef IMPORT_MODE_WAD
unsigned short checksum;
#endif

FILE *in_file = 0;
FILE *out_file = 0;

enum{
	//					32X							JAG
	THINGS,		// 0	compressed					compressed
	LINEDEFS,	// 1	compressed					compressed
	SIDEDEFS,	// 2	compressed					compressed
	VERTEXES,	// 3	big-endian, long words		compressed
	SEGS,		// 4	compressed					compressed
	SSECTORS,	// 5	compressed					compressed
	NODES,		// 6	big-endian, long words		compressed
	SECTORS,	// 7	compressed					compressed
	REJECT,		// 8	(no differences)			compressed
	BLOCKMAP	// 9	big-endian					compressed
};

void Create_32X_();		// 32X conversion info lump
void Read_32X_(int lump);

void ConvertMapData(int lump, char mapfile);
void ConvertMapData32X(int lump, char mapfile);

void ConvertCOLORMAPData(int lump);
void ConvertCOLORMAPData32X(int lump);
void ConvertPLAYPALData(int lump);
void ConvertTEXTURE1Data(int lump);

void WritePatchName(char *name);
void CreatePNAMES(int lump);
void CreateTEXTURE1(int lump);

void ConvertRawData(int lump);
void ConvertFloorData(int lump);
void ConvertTextureData(int lump);
void ConvertTextureData32X(int lump);
void ConvertSpriteData(int lump);
void ConvertSpriteData32X(int lump);
void WriteTable(int lump, int ptr, int size);
void WriteTableCustom(int ptr, int size, unsigned char *name);
void WriteTableStart(char type);
void WriteTableEnd(char type);
void WriteTexture1(short x_size, short y_size, unsigned char *name);
int swap_endian(unsigned int i);

enum{
	TYPE_UNDEFINED,
	TYPE_TEXTURE,
	TYPE_FLOOR,
	TYPE_SPRITE,
};

char type = TYPE_UNDEFINED;

void MarsIn();
void MarsOut();

enum{
	OUTPUT_WAD,
	OUTPUT_BIN,
};

enum{
	MARS_2_PC,
	PC_2_MARS,
};
char conversion_task;

void Shutdown();



int wad32x_main(int argc, char *argv[]){
	label = 0;
	lump_count = 0;
	table_ptr = 0;
	file_size = 0;
	iwad_ptr = 0;

	type = TYPE_UNDEFINED;

	lump_value_fix = 0;
	num_of_textures = 0;

	patches_count = 0;

	texture1_ptr = 0;
	texture1_size = 0;

	first_sprite = 1;
	first_texture = 1;
	first_floor = 1;

	texture_end_written = 0;
	floor_end_written = 0;
	sprite_end_written = 0;

	out_file_size = 0xC;
	out_table_size = 0;
	out_table_ptr = 0;	// used temporarily
	out_lump_count = 0;

	d = t = 0;

	i = n = 0;

	format = 0;
	
	#ifdef WADTYPE_JAG
	printf(
		"---------------------------------\n"
		" DOOM JAGUAR WAD CONVERTER v%01.02f\n"
		" Written by Damian Grove\n"
		" %s\n"
		"---------------------------------\n"
		"\n", VERSION, __DATE__
	);
	#else
	printf(
		"------------------------------\n"
		" DOOM 32X WAD CONVERTER v%01.02f\n"
		" Written by Damian Grove\n"
		" %s\n"
		"------------------------------\n"
		"\n", VERSION, __DATE__
	);
	#endif
	
	if(argc < 4){
		printf(
			"USAGE: wad32x.exe [command] [in file] [out file]\n"
			"  example 1:  wad32x.exe -export doom32x.bin doompc.wad\n"
			"  example 2:  wad32x.exe -import doompc.wad doom32x.bin\n\n"
		);
		system("PAUSE");
		Shutdown();
	}
	
	
	
	i = strlen(argv[1]) - 1;
	
	while(i > 0){
		if(argv[1][i] >= 'A' && argv[1][i] <= 'Z')
			argv[1][i] += 0x20;
		i--;
	}
	
	
	
	if(strcmp(argv[1], "-export") == 0){
		in_file = fopen(argv[2], "rb");
		if(!in_file){
			printf("ERROR: ROM file not found.\n");
			Shutdown();		
		}
		
		out_file = fopen(argv[3], "wb");
		if(!out_file){
			printf("ERROR: Unable to create output file.\n");
			Shutdown();		
		}
		
		MarsOut();						
	}
	else if(strcmp(argv[1], "-import") == 0){
		in_file = fopen(argv[2], "rb");
		if(!in_file){
			printf("ERROR: WAD file not found.\n");
			Shutdown();
		}
		
#ifdef IMPORT_MODE_WAD
		out_file = fopen(argv[3], "wb");	// USE THIS FOR OUTPUT_WAD
		if(!out_file){
			printf("ERROR: Unable to create output file.\n");
			Shutdown();
		}
#else
		out_file = fopen(argv[3], "r+b");	// USE THIS FOR OUTPUT_BIN
		if(!out_file){
			printf("ERROR: Unable to open output file.\n");
			Shutdown();
		}
#endif
		
		MarsIn();
		Shutdown();
	}
	else{
		printf("ERROR: Invalid command.\n");
		Shutdown();		
	}
	
	return 0;
}



void Shutdown(){
	if(data)
	{
		free(data);
		data = NULL;
	}
	if(table)
	{
		free(table);
		table = NULL;
	}
	if(texture1)
	{
		free(texture1);
		texture1 = NULL;
	}
	if(out_texture1_data)
	{
		free(out_texture1_data);
		out_texture1_data = NULL;
	}
	if(out_texture1_table)
	{
		free(out_texture1_table);
		out_texture1_table = NULL;
	}
	if(pnames_table)
	{
		free(pnames_table);
		pnames_table = NULL;
	}
	if(out_table)
	{
		free(out_table);
		out_table = NULL;
	}
	if(in_file)
	{
		fclose(in_file);
		in_file = NULL;
	}
	if(out_file)
	{
		fclose(out_file);
		out_file = NULL;
	}

	//exit(0);		
}



void MarsIn(){
	// Conversion: [32X] <<---- [PC]
	
	int lump;
	
	conversion_task = PC_2_MARS;
	
	
	
	fseek(in_file, 0, SEEK_END);
	file_size = ftell(in_file);
	rewind(in_file);
	
	fread(&label, 4, 1, in_file);
	if(label == 0x44415749 || label == 0x44415750){
		// "DAWI" or "DAWP" found
		fread(&lump_count, 4, 1, in_file);
		fread(&table_ptr, 4, 1, in_file);
		if(table_ptr < 0xC || table_ptr > file_size)
			label = 0;
	}
	
	if(label != 0x44415749 && label != 0x44415750){
			// "DAWI" or "DAWP" not found
		printf("ERROR: WAD data not found.\n");
		system("PAUSE");
		return;
	}
	
	
	
	out_table = (char *)malloc(1);
	
	
	
	lump_count += 3;	// fix the patch count
	
	
	
	data = (char *)malloc(table_ptr - 0xC);
	fread(data, 1, table_ptr - 0xC, in_file);
	table = (char *)malloc(lump_count * 16);
	for(i=0; i < lump_count; i++){
		fread(&table[(i*16)], 4, 1, in_file);		// ptr
		*(int *)&table[(i*16)] = swap_endian(*(int *)&table[(i*16)]);
		
		fread(&table[(i*16)+4], 4, 1, in_file);		// size
		*(int *)&table[(i*16)+4] = swap_endian(*(int *)&table[(i*16)+4]);
		
		fread(&table[(i*16)+8], 1, 8, in_file);		// name
	}
	
	for(i=0; i < lump_count; i++){
		*(int *)&table[i*16] = swap_endian(*(int *)&table[i*16]);
		*(int *)&table[(i*16)+4] = swap_endian(*(int *)&table[(i*16)+4]);
		
		if(table[(i*16)+8] == 'P'){
			if(table[(i*16)+9] == 'L'){
				if(table[(i*16)+10] == 'A'){
					if(table[(i*16)+11] == 'Y'){
						if(table[(i*16)+12] == 'P'){
							if(table[(i*16)+13] == 'A'){
								if(table[(i*16)+14] == 'L'){
									if(table[(i*16)+15] == 0){
										table[(i*16)+15] = 'S';
									}
								}
							}
						}
					}
				}
			}
		}
	}
	fclose(in_file);
	in_file = NULL;
	
	
	
#ifdef IMPORT_MODE_WAD
	label = 0x44415749;	// make sure we're creating an IWAD
	fwrite(&label, 4, 1, out_file);
	fseek(out_file, 8, SEEK_CUR);
	iwad_ptr = 0;
#else
	fseek(out_file, 0, SEEK_END);
	out_file_size = ftell(out_file);
	rewind(out_file);
	
	for(i=4; i < out_file_size; i += 4){
		// I don't know why 'i' has to start as 4. To my logic, it should be 0.		
		label = 0;
		fread(&label, 4, 1, out_file);
		if(label == 0x44415749){
			// "DAWI" found
			fread(&out_lump_count, 4, 1, out_file);
			out_lump_count = swap_endian(out_lump_count);
			if(out_lump_count > 0 && out_lump_count <= 2048){
				// limit based on "MAXLUMPS" in Jaguar source
				fread(&out_table_ptr, 4, 1, out_file);
				out_table_ptr = swap_endian(out_table_ptr);
				if(out_table_ptr >= 0xC && out_table_ptr < 0x400000){
					// limit of 4MB based on maximum Genesis ROM size
					iwad_ptr = i;
					fseek(out_file, i + 0xC, SEEK_SET);	// required because this is using "r+b"
					out_file_size = 0;
				}
			}
		}
	}
	
	if(label != 0x44415749){
		// "DAWI" not found"
		printf("ERROR: WAD data not found.\n");
		system("PAUSE");
		return;
	}
#endif
	
	out_file_size = 0xC;
	out_lump_count = lump_count;
	
	
	
	for(lump=0; lump < lump_count-3; lump++){
		printf("0x%04X\t%c%c%c%c%c%c%c%c\n", lump + lump_value_fix,
		 (table[(lump*16)+8] & 0x7F), table[(lump*16)+9], table[(lump*16)+10], table[(lump*16)+11],
		 table[(lump*16)+12], table[(lump*16)+13], table[(lump*16)+14], table[(lump*16)+15]);
		switch(type){
			case TYPE_TEXTURE:
				if(strcmp(&table[(lump*16)+8], "P_END") == 0){
					WriteTableEnd(TYPE_TEXTURE);
					type = 0;
					if(floor_end_written){
						lump_value_fix++;
						printf("0x%04X\tTEXTURE1\n", lump + lump_value_fix);
						CreateTEXTURE1(lump);
					}
				}
				else if(table[(lump*16)+8] == 'P' && table[(lump*16)+9] == '1'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'S'
				 && table[(lump*16)+12] == 'T' && table[(lump*16)+13] == 'A'
				 && table[(lump*16)+14] == 'R' && table[(lump*16)+15] == 'T'){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'P' && table[(lump*16)+9] == '1'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'N' && table[(lump*16)+13] == 'D'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'P' && table[(lump*16)+9] == '2'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'S'
				 && table[(lump*16)+12] == 'T' && table[(lump*16)+13] == 'A'
				 && table[(lump*16)+14] == 'R' && table[(lump*16)+15] == 'T'){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'P' && table[(lump*16)+9] == '2'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'N' && table[(lump*16)+13] == 'D'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else{
					ConvertTextureData32X(lump);
				}
				break;
			case TYPE_FLOOR:
				if(strcmp(&table[(lump*16)+8], "F_END") == 0){
					WriteTableEnd(TYPE_FLOOR);
					type = 0;
					if(texture_end_written){
						lump_value_fix++;
						printf("0x%04X\tTEXTURE1\n", lump + lump_value_fix);
						CreateTEXTURE1(lump);
					}
				}
				else if(table[(lump*16)+8] == 'F' && table[(lump*16)+9] == '1'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'S'
				 && table[(lump*16)+12] == 'T' && table[(lump*16)+13] == 'A'
				 && table[(lump*16)+14] == 'R' && table[(lump*16)+15] == 'T'){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'F' && table[(lump*16)+9] == '1'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'N' && table[(lump*16)+13] == 'D'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'F' && table[(lump*16)+9] == '2'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'S'
				 && table[(lump*16)+12] == 'T' && table[(lump*16)+13] == 'A'
				 && table[(lump*16)+14] == 'R' && table[(lump*16)+15] == 'T'){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'F' && table[(lump*16)+9] == '2'
				 && table[(lump*16)+10] == '_' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'N' && table[(lump*16)+13] == 'D'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else
					ConvertFloorData(lump);
				break;
			case TYPE_SPRITE:
				if(strcmp(&table[(lump*16)+8], "S_END") == 0){
					//WriteTableEnd(TYPE_SPRITE);
					lump_value_fix--;
					type = 0;
				}
				else{
					ConvertSpriteData32X(lump);
					lump_value_fix++;
					out_lump_count++;
				}
				break;
			default:
				if(strcmp(&table[(lump*16)+8], "P_START") == 0){
					WriteTableStart(TYPE_TEXTURE);
					type = TYPE_TEXTURE;
				}
				else if(strcmp(&table[(lump*16)+8], "F_START") == 0){
					WriteTableStart(TYPE_FLOOR);
					type = TYPE_FLOOR;
				}
				else if(strcmp(&table[(lump*16)+8], "S_START") == 0){
					//WriteTableStart(TYPE_SPRITE);
					lump_value_fix--;
					type = TYPE_SPRITE;
				}
				else if(table[(lump*16)+8] == '_' && table[(lump*16)+9] == '3'
				 && table[(lump*16)+10] == '2' && table[(lump*16)+11] == 'X'
				 && table[(lump*16)+12] == '_' && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					Read_32X_(lump);
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'C' && table[(lump*16)+9] == 'O'
				 && table[(lump*16)+10] == 'L' && table[(lump*16)+11] == 'O'
				 && table[(lump*16)+12] == 'R' && table[(lump*16)+13] == 'M'
				 && table[(lump*16)+14] == 'A' && table[(lump*16)+15] == 'P'){
					ConvertCOLORMAPData32X(lump);
				}
				else if(table[(lump*16)+8] == 'P' && table[(lump*16)+9] == 'L'
				 && table[(lump*16)+10] == 'A' && table[(lump*16)+11] == 'Y'
				 && table[(lump*16)+12] == 'P' && table[(lump*16)+13] == 'A'
				 && table[(lump*16)+14] == 'L' && table[(lump*16)+15] == 'S'){
					ConvertPLAYPALData(lump);	// used for both PC and 32X conversions
				}
				else if(table[(lump*16)+8] == 'T' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'X' && table[(lump*16)+11] == 'T'
				 && table[(lump*16)+12] == 'U' && table[(lump*16)+13] == 'R'
				 && table[(lump*16)+14] == 'E' && table[(lump*16)+15] == '1'){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'T' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'X' && table[(lump*16)+11] == 'T'
				 && table[(lump*16)+12] == 'U' && table[(lump*16)+13] == 'R'
				 && table[(lump*16)+14] == 'E' && table[(lump*16)+15] == '2'){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'P' && table[(lump*16)+9] == 'N'
				 && table[(lump*16)+10] == 'A' && table[(lump*16)+11] == 'M'
				 && table[(lump*16)+12] == 'E' && table[(lump*16)+13] == 'S'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				// exclude music
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == '_'){
					lump_value_fix--;
				}
				// exclude sound (pcm)
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == 'S'){
					lump_value_fix--;
				}
				// exclude sound (pc speaker)
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == 'P'){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'M' && table[(lump*16)+11] == 'O'
				 && table[(lump*16)+12] == '1' && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'M' && table[(lump*16)+11] == 'O'
				 && table[(lump*16)+12] == '2' && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'M' && table[(lump*16)+11] == 'O'
				 && table[(lump*16)+12] == '3' && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'M' && table[(lump*16)+11] == 'O'
				 && table[(lump*16)+12] == '4' && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'E' && table[(lump*16)+9] == 'N'
				 && table[(lump*16)+10] == 'D' && table[(lump*16)+11] == 'O'
				 && table[(lump*16)+12] == 'O' && table[(lump*16)+13] == 'M'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'G' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'N' && table[(lump*16)+11] == 'M'
				 && table[(lump*16)+12] == 'I' && table[(lump*16)+13] == 'D'
				 && table[(lump*16)+14] == 'I' && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'D' && table[(lump*16)+9] == 'M'
				 && table[(lump*16)+10] == 'X' && table[(lump*16)+11] == 'G'
				 && table[(lump*16)+12] == 'U' && table[(lump*16)+13] == 'S'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					lump_value_fix--;
				}
				else if(table[(lump*16)+8] == 'T' && table[(lump*16)+9] == 'H'
				 && table[(lump*16)+10] == 'I' && table[(lump*16)+11] == 'N'
				 && table[(lump*16)+12] == 'G' && table[(lump*16)+13] == 'S'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData32X(lump, THINGS);
				}
				else if(table[(lump*16)+8] == 'L' && table[(lump*16)+9] == 'I'
				 && table[(lump*16)+10] == 'N' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'D' && table[(lump*16)+13] == 'E'
				 && table[(lump*16)+14] == 'F' && table[(lump*16)+15] == 'S'){
					ConvertMapData32X(lump, LINEDEFS);
				}
				else if(table[(lump*16)+8] == 'S' && table[(lump*16)+9] == 'I'
				 && table[(lump*16)+10] == 'D' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'D' && table[(lump*16)+13] == 'E'
				 && table[(lump*16)+14] == 'F' && table[(lump*16)+15] == 'S'){
					ConvertMapData32X(lump, SIDEDEFS);
				}
				else if(table[(lump*16)+8] == 'V' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'R' && table[(lump*16)+11] == 'T'
				 && table[(lump*16)+12] == 'E' && table[(lump*16)+13] == 'X'
				 && table[(lump*16)+14] == 'E' && table[(lump*16)+15] == 'S'){
					ConvertMapData32X(lump, VERTEXES);
				}
				else if(table[(lump*16)+8] == 'S' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'G' && table[(lump*16)+11] == 'S'
				 && table[(lump*16)+12] == 0 && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData32X(lump, SEGS);
				}
				else if(table[(lump*16)+8] == 'S' && table[(lump*16)+9] == 'S'
				 && table[(lump*16)+10] == 'E' && table[(lump*16)+11] == 'C'
				 && table[(lump*16)+12] == 'T' && table[(lump*16)+13] == 'O'
				 && table[(lump*16)+14] == 'R' && table[(lump*16)+15] == 'S'){
					ConvertMapData32X(lump, SSECTORS);
				}
				else if(table[(lump*16)+8] == 'N' && table[(lump*16)+9] == 'O'
				 && table[(lump*16)+10] == 'D' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'S' && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData32X(lump, NODES);
				}
				else if(table[(lump*16)+8] == 'S' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'C' && table[(lump*16)+11] == 'T'
				 && table[(lump*16)+12] == 'O' && table[(lump*16)+13] == 'R'
				 && table[(lump*16)+14] == 'S' && table[(lump*16)+15] == 0){
					ConvertMapData32X(lump, SECTORS);
				}
				else if(table[(lump*16)+8] == 'R' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'J' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'C' && table[(lump*16)+13] == 'T'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData32X(lump, REJECT);
				}
				else if(table[(lump*16)+8] == 'B' && table[(lump*16)+9] == 'L'
				 && table[(lump*16)+10] == 'O' && table[(lump*16)+11] == 'C'
				 && table[(lump*16)+12] == 'K' && table[(lump*16)+13] == 'M'
				 && table[(lump*16)+14] == 'A' && table[(lump*16)+15] == 'P'){
					ConvertMapData32X(lump, BLOCKMAP);
				}
				else{
					ConvertRawData(lump);
				}
				break;
		}
	}
	
#ifndef IMPORT_MODE_WAD
	out_lump_count -= 7;
#else
	out_lump_count -= 6;
#endif

#ifndef FORCE_LITTLE_ENDIAN
	i = swap_endian(ftell(out_file) - iwad_ptr);
	out_lump_count = swap_endian(out_lump_count);
#else
	i = ftell(out_file) - iwad_ptr;
#endif
	
	fwrite(out_table, 1, out_table_size, out_file);
	
#ifndef IMPORT_MODE_WAD
	printf("\n");
	
	// Adjust ROM size
	n = ftell(out_file);
	printf("ROM size = %i.%iMB\n", n>>20, (int)(((float)((n >> 16) & 0xF) / 16.0f) * 10));
	while((n & 0xFFFFF) != 0){
		fputc(0xFF, out_file);
		n++;
	}
	fseek(out_file, 0x1A4, SEEK_SET);
	n--;
	n = swap_endian(n);
	fwrite(&n, 4, 1, out_file);
	n = swap_endian(n);
	n++;
	printf("File size = %i.%iMB\n", n>>20, (int)(((float)((n >> 16) & 0xF) / 16.0f) * 10));
	if(n > 0x400000)
		printf("WARNING: ROM exceeds the Sega Genesis ROM size limitation of 4MB.\n");
#endif
	
	fseek(out_file, iwad_ptr + 4, SEEK_SET);
	fwrite(&out_lump_count, 4, 1, out_file);
	fwrite(&i, 4, 1, out_file);
	
#ifndef IMPORT_MODE_WAD
	// Fix checksum
	checksum = 0;
	fseek(out_file, 0x200, SEEK_SET);
	for(t=0x200; t < n; t += 2){
		fread(&d, 2, 1, out_file);
		d = (swap_endian(d) >> 16);
		checksum += d;
	}
	fseek(out_file, 0x18E, SEEK_SET);
	checksum = (swap_endian(checksum) >> 16);
	fwrite(&checksum, 2, 1, out_file);
	checksum = (swap_endian(checksum) >> 16);
	printf("Checksum = 0x%04X\n", checksum);
#endif
	
	
	
	printf("\nImport finished!\n");
	system("PAUSE");	
	return;
}



void MarsOut(){
	// Conversion: [32X] ---->> [PC]
	
	int lump;
	
	conversion_task = MARS_2_PC;
	
	
	
	fseek(in_file, 0, SEEK_END);
	file_size = ftell(in_file);
	rewind(in_file);
	
	for(i=0; i < file_size; i += 4){
		fread(&label, 4, 1, in_file);
		if(label == 0x44415749 || label == 0x44415750){
			// "DAWI" or "DAWP" found
			fread(&lump_count, 4, 1, in_file);
			lump_count = swap_endian(lump_count);
			if(lump_count > 0 && lump_count <= 2048){
				// limit based on "MAXLUMPS" in Jaguar source
				fread(&table_ptr, 4, 1, in_file);
				table_ptr = swap_endian(table_ptr);
				if(table_ptr >= 0xC && table_ptr < 0x400000){
					// limit of 4MB based on maximum Genesis ROM size
					iwad_ptr = i;
					file_size -= i;
					i = file_size;
				}
			}
		}
	}
	
	if(label != 0x44415749 && label != 0x44415750){
		// "DAWI" or "DAWP" not found"
		printf("ERROR: WAD data not found.\n");
		system("PAUSE");
		return;
	}
	
	
	
	out_table = (char *)malloc(1);
	
	
	
	lump_count += 2;	// fix the patch count
	lump_count++;	// make room for PNAMES
	
	
	
	data = (char *)malloc(table_ptr - 0xC);
	fread(data, 1, table_ptr - 0xC, in_file);
	table = (char *)malloc(lump_count * 16);
	fread(table, 1, lump_count * 16, in_file);
	for(i=0; i < lump_count; i++){
		*(int *)&table[i*16] = swap_endian(*(int *)&table[i*16]);
		*(int *)&table[(i*16)+4] = swap_endian(*(int *)&table[(i*16)+4]);
		
		// this part is needed to get size information about textures
		if(table[(i*16)+8] == 'T'){
			if(table[(i*16)+9] == 'E'){
				if(table[(i*16)+10] == 'X'){
					if(table[(i*16)+11] == 'T'){
						if(table[(i*16)+12] == 'U'){
							if(table[(i*16)+13] == 'R'){
								if(table[(i*16)+14] == 'E'){
									if(table[(i*16)+15] == '1'){
										texture1_ptr = *(int *)&table[i*16];
										texture1_size = *(int *)&table[(i*16)+4];
										
										fseek(in_file, iwad_ptr + texture1_ptr, SEEK_SET);
										texture1 = (char *)malloc(texture1_size);
										fread(texture1, 1, texture1_size, in_file);
									}
								}
							}
						}
					}
				}
			}
		}
		else if(table[(i*16)+8] == 'P'){
			if(table[(i*16)+9] == 'L'){
				if(table[(i*16)+10] == 'A'){
					if(table[(i*16)+11] == 'Y'){
						if(table[(i*16)+12] == 'P'){
							if(table[(i*16)+13] == 'A'){
								if(table[(i*16)+14] == 'L'){
									if(table[(i*16)+15] == 'S'){
										table[(i*16)+15] = 0;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	fclose(in_file);
	in_file = NULL;
	
	
	
	label = 0x44415750;	// make sure we're creating an PWAD
	fwrite(&label, 4, 1, out_file);
	fseek(out_file, 8, SEEK_CUR);
	
	out_lump_count = lump_count;
	
	
	
	// Put 32X label in WAD file
	Create_32X_();
	lump_value_fix++;
	out_lump_count++;
	
	
	
	for(lump=0; lump < lump_count-3; lump++){
		printf("0x%04X\t%c%c%c%c%c%c%c%c\n", lump + lump_value_fix,
		 (table[(lump*16)+8] & 0x7F), table[(lump*16)+9], table[(lump*16)+10], table[(lump*16)+11],
		 table[(lump*16)+12], table[(lump*16)+13], table[(lump*16)+14], table[(lump*16)+15]);
		switch(type){
			case TYPE_TEXTURE:
				if(strcmp(&table[(lump*16)+8], "T_END") == 0){
					WriteTableEnd(TYPE_TEXTURE);
					type = 0;
				}
				else{
					ConvertTextureData(lump);
				}
				break;
			case TYPE_FLOOR:
				if(strcmp(&table[(lump*16)+8], "F_END") == 0){
					WriteTableEnd(TYPE_FLOOR);
					type = 0;
				}
				else
					ConvertFloorData(lump);
				break;
			default:
				if(lump < lump_count-1){
					if(table[(lump*16)+8+16] == '.' && table[(lump*16)+9+16] == 0){
						if(first_sprite){
							if(!first_texture){
								WriteTableEnd(TYPE_TEXTURE);
								first_texture = 1;
							}
							else if(!first_floor){
								WriteTableEnd(TYPE_FLOOR);
								first_floor = 1;
							}
							first_sprite = 0;
							WriteTableStart(TYPE_SPRITE);
						}
						ConvertSpriteData(lump);
						lump++;
						out_lump_count--;
					}
					else if(strcmp(&table[(lump*16)+9], "_START") == 0){
						if(table[(lump*16)+8] == 'T'){
							if(!first_floor){
								WriteTableEnd(TYPE_FLOOR);
								first_floor = 1;
							}
							else if(!first_sprite){
								WriteTableEnd(TYPE_SPRITE);
								first_sprite = 1;
							}
							WriteTableStart(TYPE_TEXTURE);
							type = TYPE_TEXTURE;
						}
						else if(table[(lump*16)+8] == 'F'){
							if(!first_floor){
								WriteTableEnd(TYPE_TEXTURE);
								first_texture = 1;
							}
							else if(!first_sprite){
								WriteTableEnd(TYPE_SPRITE);
								first_sprite = 1;
							}
							WriteTableStart(TYPE_FLOOR);
							type = TYPE_FLOOR;
						}
					}
					else if(strcmp(&table[(lump*16)+9], "_END") == 0){
						if(table[(lump*16)+8] == 'T')
							WriteTableEnd(TYPE_TEXTURE);
						else if(table[(lump*16)+8] == 'F')
							WriteTableEnd(TYPE_FLOOR);
						type = 0;
					}
					else if(table[(lump*16)+8] == 'C' && table[(lump*16)+9] == 'O'
					 && table[(lump*16)+10] == 'L' && table[(lump*16)+11] == 'O'
					 && table[(lump*16)+12] == 'R' && table[(lump*16)+13] == 'M'
					 && table[(lump*16)+14] == 'A' && table[(lump*16)+15] == 'P'){
						ConvertCOLORMAPData(lump);
					}
					else if(table[(lump*16)+8] == 'P' && table[(lump*16)+9] == 'L'
					 && table[(lump*16)+10] == 'A' && table[(lump*16)+11] == 'Y'
					 && table[(lump*16)+12] == 'P' && table[(lump*16)+13] == 'A'
					 && table[(lump*16)+14] == 'L' && table[(lump*16)+15] == 0){
						ConvertPLAYPALData(lump);
					}
					else if(table[(lump*16)+8] == 'T' && table[(lump*16)+9] == 'E'
					 && table[(lump*16)+10] == 'X' && table[(lump*16)+11] == 'T'
					 && table[(lump*16)+12] == 'U' && table[(lump*16)+13] == 'R'
					 && table[(lump*16)+14] == 'E' && table[(lump*16)+15] == '1'){
						ConvertTEXTURE1Data(lump);
						lump_value_fix++;
						printf("0x%04X\tPNAMES\n", lump + lump_value_fix);
						CreatePNAMES(lump);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'T' && table[(lump*16)+9] == 'H'
					 && table[(lump*16)+10] == 'I' && table[(lump*16)+11] == 'N'
					 && table[(lump*16)+12] == 'G' && table[(lump*16)+13] == 'S'
					 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
						ConvertMapData(lump, THINGS);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'L' && table[(lump*16)+9] == 'I'
					 && table[(lump*16)+10] == 'N' && table[(lump*16)+11] == 'E'
					 && table[(lump*16)+12] == 'D' && table[(lump*16)+13] == 'E'
					 && table[(lump*16)+14] == 'F' && table[(lump*16)+15] == 'S'){
						ConvertMapData(lump, LINEDEFS);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'I'
					 && table[(lump*16)+10] == 'D' && table[(lump*16)+11] == 'E'
					 && table[(lump*16)+12] == 'D' && table[(lump*16)+13] == 'E'
					 && table[(lump*16)+14] == 'F' && table[(lump*16)+15] == 'S'){
						ConvertMapData(lump, SIDEDEFS);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'V' && table[(lump*16)+9] == 'E'
					 && table[(lump*16)+10] == 'R' && table[(lump*16)+11] == 'T'
					 && table[(lump*16)+12] == 'E' && table[(lump*16)+13] == 'X'
					 && table[(lump*16)+14] == 'E' && table[(lump*16)+15] == 'S'){
						ConvertMapData(lump, VERTEXES);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'E'
					 && table[(lump*16)+10] == 'G' && table[(lump*16)+11] == 'S'
					 && table[(lump*16)+12] == 0 && table[(lump*16)+13] == 0
					 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
						ConvertMapData(lump, SEGS);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'S'
					 && table[(lump*16)+10] == 'E' && table[(lump*16)+11] == 'C'
					 && table[(lump*16)+12] == 'T' && table[(lump*16)+13] == 'O'
					 && table[(lump*16)+14] == 'R' && table[(lump*16)+15] == 'S'){
						ConvertMapData(lump, SSECTORS);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'N' && table[(lump*16)+9] == 'O'
					 && table[(lump*16)+10] == 'D' && table[(lump*16)+11] == 'E'
					 && table[(lump*16)+12] == 'S' && table[(lump*16)+13] == 0
					 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
						ConvertMapData(lump, NODES);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'E'
					 && table[(lump*16)+10] == 'C' && table[(lump*16)+11] == 'T'
					 && table[(lump*16)+12] == 'O' && table[(lump*16)+13] == 'R'
					 && table[(lump*16)+14] == 'S' && table[(lump*16)+15] == 0){
						ConvertMapData(lump, SECTORS);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'R' && table[(lump*16)+9] == 'E'
					 && table[(lump*16)+10] == 'J' && table[(lump*16)+11] == 'E'
					 && table[(lump*16)+12] == 'C' && table[(lump*16)+13] == 'T'
					 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
						ConvertMapData(lump, REJECT);
					}
					else if((table[(lump*16)+8] & 0x7F) == 'B' && table[(lump*16)+9] == 'L'
					 && table[(lump*16)+10] == 'O' && table[(lump*16)+11] == 'C'
					 && table[(lump*16)+12] == 'K' && table[(lump*16)+13] == 'M'
					 && table[(lump*16)+14] == 'A' && table[(lump*16)+15] == 'P'){
						ConvertMapData(lump, BLOCKMAP);
					}
					else{
						if(!first_texture){
							WriteTableEnd(TYPE_TEXTURE);
							first_texture = 1;
						}
						else if(!first_floor){
							WriteTableEnd(TYPE_FLOOR);
							first_floor = 1;
						}
						else if(!first_sprite){
							WriteTableEnd(TYPE_SPRITE);
							first_sprite = 1;
						}
						ConvertRawData(lump);
					}
				}
				else if(table[(lump*16)+8] == 'T' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'X' && table[(lump*16)+11] == 'T'
				 && table[(lump*16)+12] == 'U' && table[(lump*16)+13] == 'R'
				 && table[(lump*16)+14] == 'E' && table[(lump*16)+15] == '1'){
					ConvertRawData(lump);
					lump_value_fix++;
					printf("0x%04X\tPNAMES\n", lump + lump_value_fix);
					CreatePNAMES(lump);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'T' && table[(lump*16)+9] == 'H'
				 && table[(lump*16)+10] == 'I' && table[(lump*16)+11] == 'N'
				 && table[(lump*16)+12] == 'G' && table[(lump*16)+13] == 'S'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData(lump, THINGS);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'L' && table[(lump*16)+9] == 'I'
				 && table[(lump*16)+10] == 'N' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'D' && table[(lump*16)+13] == 'E'
				 && table[(lump*16)+14] == 'F' && table[(lump*16)+15] == 'S'){
					ConvertMapData(lump, LINEDEFS);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'I'
				 && table[(lump*16)+10] == 'D' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'D' && table[(lump*16)+13] == 'E'
				 && table[(lump*16)+14] == 'F' && table[(lump*16)+15] == 'S'){
					ConvertMapData(lump, SIDEDEFS);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'V' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'R' && table[(lump*16)+11] == 'T'
				 && table[(lump*16)+12] == 'E' && table[(lump*16)+13] == 'X'
				 && table[(lump*16)+14] == 'E' && table[(lump*16)+15] == 'S'){
					ConvertMapData(lump, VERTEXES);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'G' && table[(lump*16)+11] == 'S'
				 && table[(lump*16)+12] == 0 && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData(lump, SEGS);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'S'
				 && table[(lump*16)+10] == 'E' && table[(lump*16)+11] == 'C'
				 && table[(lump*16)+12] == 'T' && table[(lump*16)+13] == 'O'
				 && table[(lump*16)+14] == 'R' && table[(lump*16)+15] == 'S'){
					ConvertMapData(lump, SSECTORS);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'N' && table[(lump*16)+9] == 'O'
				 && table[(lump*16)+10] == 'D' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'S' && table[(lump*16)+13] == 0
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData(lump, NODES);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'S' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'C' && table[(lump*16)+11] == 'T'
				 && table[(lump*16)+12] == 'O' && table[(lump*16)+13] == 'R'
				 && table[(lump*16)+14] == 'S' && table[(lump*16)+15] == 0){
					ConvertMapData(lump, SECTORS);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'R' && table[(lump*16)+9] == 'E'
				 && table[(lump*16)+10] == 'J' && table[(lump*16)+11] == 'E'
				 && table[(lump*16)+12] == 'C' && table[(lump*16)+13] == 'T'
				 && table[(lump*16)+14] == 0 && table[(lump*16)+15] == 0){
					ConvertMapData(lump, REJECT);
				}
				else if((table[(lump*16)+8] & 0x7F) == 'B' && table[(lump*16)+9] == 'L'
				 && table[(lump*16)+10] == 'O' && table[(lump*16)+11] == 'C'
				 && table[(lump*16)+12] == 'K' && table[(lump*16)+13] == 'M'
				 && table[(lump*16)+14] == 'A' && table[(lump*16)+15] == 'P'){
					ConvertMapData(lump, BLOCKMAP);
				}
				else{
					ConvertRawData(lump);
				}
				break;
		}
	}
	
	
	
	i = ftell(out_file);
	fwrite(out_table, 1, out_table_size, out_file);
	
	fseek(out_file, 4, SEEK_SET);
	fwrite(&out_lump_count, 4, 1, out_file);
	fwrite(&i, 4, 1, out_file);
	
	
	
	printf("\nExport finished!\n");
	system("PAUSE");	
	return;
}



void Read_32X_(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	if(data_size < 9){
		format = 0;
	}
	else{
		if(
		 data[data_ptr+0] == 'W' &&
		 data[data_ptr+1] == 'A' &&
		 data[data_ptr+2] == 'D' &&
		 data[data_ptr+3] == '3' &&
		 data[data_ptr+4] == '2' &&
		 data[data_ptr+5] == 'X'
		){
			format = data[data_ptr + 8];
			if(format > WAD_FORMAT){
				printf("WARNING: WAD format %i not supported. Assuming format %i.\n", format, WAD_FORMAT);
			}
		}
		else{
			format = 0;
		}
	}
}



void Create_32X_(){
	char string[6] = { 'W', 'A', 'D', '3', '2', 'X' };
	short ver = (short)(VERSION * 100);
	short format = WAD_FORMAT;
	
	fwrite(string, 1, 6, out_file);
	fwrite(&ver, 2, 1, out_file);
	fwrite(&format, 2, 1, out_file);
	
	WriteTableCustom(out_file_size, 10, "_32X_");
	
	out_file_size += 10;
}



void CreatePNAMES(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	char name[9];
	
	int i;
	
	name[8] = '\0';
	
	for(i=0x12C; i < data_size; i += 0x20){
		name[0] = data[data_ptr+i];
		name[1] = data[data_ptr+i+1];
		name[2] = data[data_ptr+i+2];
		name[3] = data[data_ptr+i+3];
		name[4] = data[data_ptr+i+4];
		name[5] = data[data_ptr+i+5];
		name[6] = data[data_ptr+i+6];
		name[7] = data[data_ptr+i+7];
		WritePatchName(name);
	}
	
	fwrite(pnames_table, 1, 4+(patches_count<<3), out_file);
	
	WriteTableCustom(out_file_size, 4+(patches_count<<3), "PNAMES");
	
	out_file_size += (4+(patches_count<<3));
}



void CreateTEXTURE1(int lump){
	// Before writing the data, we sort the textures in ABC order purely for
	// convention purposes.
	unsigned long long *texture1_names;
	unsigned short *texture1_order;
	
	if(num_of_textures > 0){
		texture1_names = (long long *)malloc(num_of_textures<<3);
		texture1_order = (short *)calloc(num_of_textures, 2);
	}
	
	for(i=0; i < num_of_textures; i++){
		//texture1_names[i] = *(long long *)&out_texture1_data[i<<5];
		
		texture1_names[i] = out_texture1_data[(i<<5)+0];
		texture1_names[i] <<= 8;
		texture1_names[i] |= out_texture1_data[(i<<5)+1];
		texture1_names[i] <<= 8;
		texture1_names[i] |= out_texture1_data[(i<<5)+2];
		texture1_names[i] <<= 8;
		texture1_names[i] |= out_texture1_data[(i<<5)+3];
		texture1_names[i] <<= 8;
		texture1_names[i] |= out_texture1_data[(i<<5)+4];
		texture1_names[i] <<= 8;
		texture1_names[i] |= out_texture1_data[(i<<5)+5];
		texture1_names[i] <<= 8;
		texture1_names[i] |= out_texture1_data[(i<<5)+6];
		texture1_names[i] <<= 8;
		texture1_names[i] |= out_texture1_data[(i<<5)+7];
	}
	
	for(i=0; i < num_of_textures; i++){
		for(n=0; n < num_of_textures; n++){
			if(texture1_names[i] > texture1_names[n]){
				texture1_order[i]++;
			}
		}
	}
	
	fwrite(out_texture1_table, 1, (num_of_textures<<2) + 4, out_file);
	
	for(i=0; i < num_of_textures; i++){
		n=0;
		while(texture1_order[n] != i)
			n++;
		
		*(short *)&out_texture1_data[(n << 5) + 26] = i;
		fwrite(&out_texture1_data[n << 5], 1, 32, out_file);
	}
	
	WriteTableCustom(out_file_size, (num_of_textures<<2) + (num_of_textures<<5) + 4, "TEXTURE1");
	
	out_file_size += (num_of_textures<<2) + (num_of_textures<<5) + 4;
	
	if(num_of_textures > 0){
		free(texture1_names);
		free(texture1_order);
	}
}



void ConvertTEXTURE1Data(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	int i;
	
	int num_of_textures = *(int *)&data[data_ptr];
	
	int *texture_ptr_table;
	char *texture_data_table;
	
	
	
	// Our task with this function is to copy TEXTURE1 with "AASHITTY" inserted
	
	
	
	texture_ptr_table = (int *)malloc((num_of_textures+1) << 2);
	texture_data_table = (char *)malloc((num_of_textures+1) << 5);
	
	
	
	// Copy PTR table
	texture_ptr_table[0] = ((num_of_textures+1)<<2) + 4;
	for(i=1; i < num_of_textures+1; i++)
		texture_ptr_table[i] = *(int *)&data[data_ptr + 4 - 4 + (i<<2)] + 32 + 4;
	
	
	
	// Copy DATA table
	texture_data_table[0] = 'A';
	texture_data_table[1] = 'A';
	texture_data_table[2] = 'S';
	texture_data_table[3] = 'H';
	texture_data_table[4] = 'I';
	texture_data_table[5] = 'T';
	texture_data_table[6] = 'T';
	texture_data_table[7] = 'Y';
	*(int *)&texture_data_table[8] = 0;
	*(short *)&texture_data_table[12] = 64;
	*(short *)&texture_data_table[14] = 128;
	*(int *)&texture_data_table[16] = 0;
	*(short *)&texture_data_table[20] = 1;
	*(short *)&texture_data_table[22] = 0;
	*(short *)&texture_data_table[24] = 0;
	*(short *)&texture_data_table[26] = 0;
	*(short *)&texture_data_table[28] = 1;
	*(short *)&texture_data_table[30] = 0;
	
	for(i=0; i < (num_of_textures<<5); i++)
		texture_data_table[32 + i] = data[data_ptr + 4 + (num_of_textures<<2) + i];
	
	
	
	num_of_textures++;
	fwrite(&num_of_textures, 4, 1, out_file);
	num_of_textures--;
	
	fwrite(texture_ptr_table, 4, num_of_textures+1, out_file);
	
	fwrite(texture_data_table, 1, (num_of_textures+1)<<5, out_file);
	
	free(texture_ptr_table);
	free(texture_data_table);
	
	WriteTable(lump, out_file_size, 4 + ((num_of_textures+1)<<2) + ((num_of_textures+1)<<5));
	
	out_file_size += (4 + ((num_of_textures+1)<<2) + ((num_of_textures+1)<<5));
}



void ConvertPLAYPALData(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	unsigned short color;
	
	for(color=0; color < 11520; color++){
		fputc(data[data_ptr + (color % data_size)], out_file);
	}
	
	WriteTable(lump, out_file_size, 11520);
	
	out_file_size += 11520;
}



void ConvertCOLORMAPData32X(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	unsigned short color;
	unsigned short shade;
	
	if(data_size == 8704){
		// COLORMAP doesn't contain the extra 32X data
		for(shade=0; shade < 32*256; shade += 256){
			for(color=128; color < 256+128; color++){
				fputc(data[data_ptr + (color % 256) + shade], out_file);
				fputc(data[data_ptr + (color % 256) + shade], out_file);
			}
		}
		
		WriteTable(lump, out_file_size, 16384);
		
		out_file_size += 16384;
	}
	else if(format == 0){
		// this is for backwards compatibility with v0.99 and v1.00 WAD files
		fwrite(&data[data_ptr], 1, data_size, out_file);
		
		WriteTable(lump, out_file_size, data_size);
		
		out_file_size += data_size;
	}
	else{
		for(shade=0; shade < 32*256; shade += 256){
			for(color=128; color < 256+128; color++){
				fputc(data[data_ptr + (color % 256) + shade], out_file);
				fputc(data[data_ptr + (color % 256) + shade + 8704], out_file);
			}
		}
		
		WriteTable(lump, out_file_size, 16384);
		
		out_file_size += 16384;
	}
}



void ConvertCOLORMAPData(int lump){
	char inv_colormap[0x100] = {
		0x04, 0x51, 0x50, 0x59, 0x00, 0x51, 0x50, 0x50, 0x04, 0x55, 0x53, 0x51, 0x50, 0x57, 0x56, 0x54,
		0x6D, 0x6C, 0x6A, 0x03, 0x68, 0x66, 0x65, 0x64, 0x62, 0x61, 0x60, 0x5F, 0x5E, 0x5D, 0x5C, 0x5B,
		0x5A, 0x59, 0x58, 0x57, 0x57, 0x56, 0x55, 0x55, 0x54, 0x53, 0x52, 0x52, 0x52, 0x51, 0x51, 0x50,
		0x07, 0x06, 0x06, 0x05, 0x6F, 0x6E, 0x6D, 0x6D, 0x6C, 0x6A, 0x69, 0x68, 0x67, 0x65, 0x64, 0x64,
		0x62, 0x61, 0x60, 0x60, 0x5F, 0x5E, 0x5C, 0x5C, 0x5B, 0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53,
		0x07, 0x06, 0x05, 0x05, 0x6E, 0x6D, 0x6D, 0x6B, 0x6A, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64, 0x63,
		0x62, 0x61, 0x60, 0x5F, 0x5E, 0x5D, 0x5C, 0x5B, 0x5A, 0x58, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53,
		0x6C, 0x6A, 0x68, 0x66, 0x64, 0x62, 0x60, 0x5F, 0x5D, 0x5B, 0x59, 0x57, 0x55, 0x53, 0x51, 0x50,
		0x68, 0x67, 0x65, 0x64, 0x63, 0x62, 0x61, 0x60, 0x5F, 0x5D, 0x5D, 0x5C, 0x5B, 0x5A, 0x58, 0x58,
		0x62, 0x60, 0x5F, 0x5D, 0x5B, 0x59, 0x57, 0x55, 0x60, 0x5F, 0x5D, 0x5C, 0x5A, 0x59, 0x57, 0x56,
		0x07, 0x6D, 0x69, 0x65, 0x61, 0x5D, 0x5A, 0x56, 0x00, 0x06, 0x6E, 0x6A, 0x66, 0x63, 0x60, 0x5C,
		0x59, 0x58, 0x57, 0x57, 0x57, 0x56, 0x55, 0x55, 0x54, 0x54, 0x53, 0x53, 0x52, 0x51, 0x51, 0x50,
		0x07, 0x6E, 0x6A, 0x66, 0x62, 0x5E, 0x5A, 0x57, 0x53, 0x52, 0x52, 0x51, 0x51, 0x50, 0x50, 0x50,
		0x00, 0x07, 0x05, 0x6E, 0x6C, 0x69, 0x67, 0x65, 0x63, 0x62, 0x61, 0x60, 0x5F, 0x5D, 0x5C, 0x5B,
		0x00, 0x00, 0x08, 0x07, 0x07, 0x06, 0x05, 0x05, 0x5B, 0x59, 0x58, 0x57, 0x57, 0x55, 0x53, 0x52,
		0x50, 0x50, 0x50, 0x04, 0x04, 0x04, 0x04, 0x04, 0x03, 0x6F, 0x69, 0x5F, 0x5B, 0x58, 0x55, 0x61
	};
	
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	//int data_size = *(int *)&table[(lump*16)+4];
	
	unsigned short color;
	unsigned short shade;
	
	// regular colors
	for(shade=0; shade < 32*256*2; shade += (256*2)){
		for(color=(128*2); color < (256+128)*2; color += 2)
			fputc(data[data_ptr + (color % (256*2)) + shade], out_file);
	}
	
	// invulnerability colors
	for(color=0; color < 256; color++)
		fputc(inv_colormap[color], out_file);
	for(color=0; color < 256; color++)
		fputc(0, out_file);
	
	// interlacing colors (for when data is ported back to the 32X)
	for(shade=0; shade < 32*256*2; shade += (256*2)){
		for(color=(128*2)+1; color < (256+128)*2; color += 2)
			fputc(data[data_ptr + (color % (256*2)) + shade], out_file);
	}
	
	WriteTable(lump, out_file_size, 16896);
	
	out_file_size += 16896;
}



void ConvertMapData32X(int lump, char mapfile){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	unsigned char *output = 0;
	int output_size = 0;
	
	unsigned char *uncompressed = 0;
	int compressed_size = 0;
	
	unsigned char *key = 0;
	int key_size = 0;
	
	unsigned char *bitfield = 0;
	int bitfield_size = 0;
	
	int n;	// 
	int i;	// 
	int b;	// 
	int c;	// number of bytes to compare
	
	int data_comp_1;
	int data_comp_2;
	char bytes_to_copy = 0;
	
	
	
	switch(mapfile){
		case THINGS:
		case LINEDEFS:
		case SIDEDEFS:
		case SEGS:
		case SSECTORS:
		case SECTORS:
			output_size = data_size;	// must be uncompressed size
			
			key = (char *)malloc(1);
			bitfield = (char *)malloc(1);
			
			table[(lump*16)+8] |= 0x80;
			
			uncompressed = (char *)malloc(data_size);
			for(i=0; i < data_size; i++){
				uncompressed[i] = data[data_ptr + i];
			}
			break;
		case VERTEXES:
			output_size = data_size<<1;
			output = (char *)malloc(data_size<<1);
			for(i=0; i < data_size<<1; i += 4){
				output[i] = data[data_ptr + (i>>1)+1];
				output[i+1] = data[data_ptr + (i>>1)];
				*(unsigned short *)&output[i+2] = 0;
			}
			break;
		case NODES:
			output_size = data_size<<1;
			output = (char *)malloc(data_size<<1);
			for(i=0; i < data_size<<1; i += 4){
				if((i % 56) >= 48){
					*(unsigned short *)&output[i] = 0;
					output[i+2] = data[data_ptr + (i>>1)+1];
					output[i+3] = data[data_ptr + (i>>1)];
				}
				else{
					output[i] = data[data_ptr + (i>>1)+1];
					output[i+1] = data[data_ptr + (i>>1)];
					*(unsigned short *)&output[i+2] = 0;
				}
			}
			break;
		case REJECT:
			output_size = data_size;
			output = (char *)malloc(data_size);
			for(i=0; i < data_size; i++){
				output[i] = data[data_ptr + i];
			}
			break;
		case BLOCKMAP:
			output_size = data_size;
			output = (char *)malloc(data_size);
			for(i=0; i < data_size; i += 2){
				output[i] = data[data_ptr + i+1];
				output[i+1] = data[data_ptr + i];
			}
			break;
	}
	
	
	
	if(table[(lump*16)+8] >> 7){
		// data needs to be compressed
		
		for(i=1; i <= output_size - 3; i++){
			// check for repeated data (no less than 3 bytes in length)
			
			data_comp_1 = (uncompressed[i]<<16)
			 | (uncompressed[i+1]<<8)
			 | uncompressed[i+2];
			 
			for(n=0; n < i; n++){
				if(i-n > 0x1000)
					n = i - 0x1000;
					
				data_comp_2 = (uncompressed[n]<<16)
				 | (uncompressed[n + (1%(i-n))]<<8)
				 | uncompressed[n + (2%(i-n))];
				
				if(data_comp_1 == data_comp_2){
					// check to see how many bytes match beyond 3
					
					if(bytes_to_copy == 0){
						bytes_to_copy = 3;
						b = n;
					}
					for(c=3; c <= 16; c++){
						if(i+c <= output_size){
							if(uncompressed[i+c] != uncompressed[n + (c%(i-n))]){
								// stop checking for matches
								if(c >= bytes_to_copy){
									bytes_to_copy = c;
									c = 0x40;
									b = n;		// save 'n' for referencing later
								}
								else
									c = 0x40;
							}
						}
						else{
							if(c >= bytes_to_copy){
								bytes_to_copy = c-1;
								c = 0x40;
								b = n;		// save 'n' for referencing later
							}
							else
								c = 0x40;
						}	
					}
					
					if(c == 17){
						bytes_to_copy = 16;
						b = n;
					}
				}
			}
			
			if(bytes_to_copy){
				// add info to the key
				
				key_size += 4;
				key = (char *)realloc(key, key_size);
				
				*(unsigned short *)&key[key_size - 4] = i;
				*(unsigned short *)&key[key_size - 2] = (((i-b)-1)<<4) | (bytes_to_copy-1);
				
				i += (bytes_to_copy-1);
				
				bytes_to_copy = 0;
			}
		}
		
		key_size += 4;
		key = (char *)realloc(key, key_size);
		
		*(unsigned short *)&key[key_size - 4] = 0xFFFF;
		*(unsigned short *)&key[key_size - 2] = 0;
		
		// create key bitfield
		n = 0;
		for(i=0; i < output_size; i++){
			if(i < *(unsigned short *)&key[n]){
				bitfield_size++;
				bitfield = (char *)realloc(bitfield, bitfield_size);
				
				bitfield[bitfield_size-1] = 0;
			}
			else{
				bitfield_size++;
				bitfield = (char *)realloc(bitfield, bitfield_size);
				
				bitfield[bitfield_size-1] = 1;
				
				i += (*(unsigned short *)&key[n+2] & 0xF);
				n += 4;
			}
		}
		
		// add terminator to key
		bitfield_size++;
		bitfield = (char *)realloc(bitfield, bitfield_size);
		bitfield[bitfield_size-1] = 0xF;
		
		// if needed, align the bitfield to the next whole byte (8 bits)
		while(bitfield_size & 7){
			bitfield_size++;
			bitfield = (char *)realloc(bitfield, bitfield_size);
			
			bitfield[bitfield_size-1] = 0;
		}
		
		// write data
		n = 0;	// used as subscript to key[]
		b = 0;	// used as subscript to bitfield[]
		for(i=0; i < output_size; i++){	
			if(i < output_size){
				if((b & 7) == 0){
					data_comp_1 = 0;
					
					if(bitfield[b] == 0xF)	data_comp_1 |= 1;
					else					data_comp_1 |= bitfield[b];
					
					if(bitfield[b+1] == 0xF)	data_comp_1 |= 2;
					else					data_comp_1 |= (bitfield[b+1]<<1);
					
					if(bitfield[b+2] == 0xF)	data_comp_1 |= 4;
					else					data_comp_1 |= (bitfield[b+2]<<2);
					
					if(bitfield[b+3] == 0xF)	data_comp_1 |= 8;
					else					data_comp_1 |= (bitfield[b+3]<<3);
					
					if(bitfield[b+4] == 0xF)	data_comp_1 |= 16;
					else					data_comp_1 |= (bitfield[b+4]<<4);
					
					if(bitfield[b+5] == 0xF)	data_comp_1 |= 32;
					else					data_comp_1 |= (bitfield[b+5]<<5);
					
					if(bitfield[b+6] == 0xF)	data_comp_1 |= 64;
					else					data_comp_1 |= (bitfield[b+6]<<6);
					
					if(bitfield[b+7] == 0xF)	data_comp_1 |= 128;
					else					data_comp_1 |= (bitfield[b+7]<<7);
					
					fputc(data_comp_1, out_file);
					compressed_size++;
				}
				
				if(data_comp_1 & 1){
					fputc(key[n+3], out_file);
					fputc(key[n+2], out_file);
					compressed_size += 2;
					
					i += (*(unsigned short *)&key[n+2] & 0xF);
					n += 4;
				}
				else{
					fputc(uncompressed[i], out_file);
					compressed_size++;
				}
				
				data_comp_1 >>= 1;
				b++;
			}	
		}
		
		if((b&7) == 0){
			if(bitfield[b] == 0xF){	
				// let 'i' go one over to allow special cases
				// where terminator needs to be inserted
				// (e.g. MAP14 "SEGS")
				b = 1;
				fputc(b, out_file);
				compressed_size++;
			}
		}
		
		// add terminator
		compressed_size += 2;
		i = 0;
		fwrite(&i, 2, 1, out_file);
		
		while(compressed_size & 3){
			fputc(i, out_file);
			compressed_size++;
		}
		
		
		
		WriteTable(lump, out_file_size, output_size);
		
		out_file_size += compressed_size;
	}
	else{
		fwrite(output, 1, output_size, out_file);
		
		i = 0;
		n = output_size;
		while(n & 3){
			fputc(i, out_file);
			n++;
		}
		
		WriteTable(lump, out_file_size, output_size);
		
		out_file_size += output_size;
		
		n = output_size;
		while(n & 3){
			n++;
			out_file_size++;
		}
	}
	
	
	
	if(output)
		free(output);
	if(uncompressed)
		free(uncompressed);
	if(key)
		free(key);
	if(bitfield)
		free(bitfield);
}



void ConvertMapData(int lump, char mapfile){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	unsigned char *uncompressed;
	unsigned char *converted;
	
	int ci;
	int i;
	
	char key;
	char key_bit;
	
	int key_location;
	char key_count;
	
	
	
	uncompressed = (char *)malloc(data_size + 16);	// add 16 to make sure we have enough room
	
	if(table[(lump*16)+8] >> 7){
		// data must be decompressed
		
		table[(lump*16)+8] &= 0x7F;
		
		ci = 0;
		i = 0;
		while(i < data_size){
			key = data[data_ptr + ci];
			ci++;
			for(key_bit = 0; key_bit < 8; key_bit++){
				if(key & 1){
					// copy data from location
					key_location = (data[data_ptr + ci]<<8) | data[data_ptr + ci+1];
					key_count = (key_location & 0xF)+1;
					key_location = i-((key_location>>4)+1);
					
					while(key_count > 0){
						uncompressed[i] = uncompressed[key_location];
						key_location++;
						key_count--;
						i++;
					}
					ci += 2;
				}
				else{
					// do a direct copy
					uncompressed[i] = data[data_ptr + ci];
					ci++;
					i++;
				}
				
				key >>= 1;
			}
		}
	}
	else{
		for(i=0; i < data_size; i++){
			uncompressed[i] = data[data_ptr + i];
		}
	}
	
	switch(mapfile){
		case THINGS:
		case LINEDEFS:
		case SIDEDEFS:
		case SEGS:
		case SSECTORS:
		case SECTORS:
	#ifdef WADTYPE_JAG
		case VERTEXES:
		case NODES:
		case REJECT:
		case BLOCKMAP:
	#endif
			converted = (char *)malloc(data_size);
			for(i=0; i < data_size; i++){
				converted[i] = uncompressed[i];
			}
			break;
	#ifndef WADTYPE_JAG
		case VERTEXES:
			data_size >>= 1;
			converted = (char *)malloc(data_size);
			for(i=0; i < data_size; i += 2){
				converted[i] = uncompressed[(i<<1)+1];
				converted[i+1] = uncompressed[(i<<1)];
			}
			break;
		case NODES:
			data_size >>= 1;
			converted = (char *)malloc(data_size);
			for(i=0; i < data_size; i += 2){
				if((i % 28) >= 24){
					converted[i] = uncompressed[(i<<1)+3];
					converted[i+1] = uncompressed[(i<<1)+2];
				}
				else{
					converted[i] = uncompressed[(i<<1)+1];
					converted[i+1] = uncompressed[(i<<1)];
				}
			}
			break;
		case REJECT:
			converted = (char *)malloc(data_size);
			for(i=0; i < data_size; i++){
				converted[i] = uncompressed[i];
			}
			break;
		case BLOCKMAP:
			converted = (char *)malloc(data_size);
			for(i=0; i < data_size; i+= 2){
				converted[i] = uncompressed[i+1];
				converted[i+1] = uncompressed[i];
			}
			break;
	#endif
	}
	
	fwrite(converted, 1, data_size, out_file);
	
	WriteTable(lump, out_file_size, data_size);
	
	out_file_size += data_size;
	
	free(uncompressed);
	free(converted);
}



void ConvertRawData(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	fwrite(&data[data_ptr], 1, data_size, out_file);
	
	WriteTable(lump, out_file_size, data_size);
	
	out_file_size += data_size;
}



void ConvertFloorData(int lump){
	// NO CONVERSION NEEDED! Just do a direct copy!
	ConvertRawData(lump);
}



void ConvertTextureData32X(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	short x_size;
	short y_size;
	int offset;
	
	short x;
	short y;
	
	unsigned char *output;
	
	char name[9];
	
	
	
	name[0] = table[(lump*16)+8];
	name[1] = table[(lump*16)+9];
	name[2] = table[(lump*16)+10];
	name[3] = table[(lump*16)+11];
	name[4] = table[(lump*16)+12];
	name[5] = table[(lump*16)+13];
	name[6] = table[(lump*16)+14];
	name[7] = table[(lump*16)+15];
	name[8] = '\0';
	
	x_size = *(short *)&data[data_ptr];
	y_size = *(short *)&data[data_ptr+2];
	
	WriteTexture1(x_size, y_size, name);
	
	
	
	offset = *(int *)&data[data_ptr+8] + 3;
	
	output = (char *)malloc((x_size * y_size) + 320);
	
	for(x=0; x < x_size; x++){
		for(y=0; y < y_size; y++){
			output[y + (x * y_size)] = data[data_ptr + offset];
			offset++;
		}
		offset += 5;
	}
	
	for(x=0; x < 320; x++){
		output[(x_size * y_size) + x] = output[x];
	}
	
	
	
	fwrite(output, 1, (x_size * y_size) + 320, out_file);
	WriteTable(lump, out_file_size, (x_size * y_size) + 320);
	
	out_file_size += ((x_size * y_size) + 320);
	
	free(output);
}



void ConvertTextureData(int lump){
	// write 8 byte header
	// write line index table
	// for each line:
		// write 0x0080
		// copy first byte in line
		// write entire line (including first byte)
		// copy last byte in line
		// write 0xFF
		// repeat these steps for the next line
	
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	unsigned char *output;
	unsigned char *line_lookup;
	
	int line_lookup_size;
	
	int array_size = 0;
	short prev_array_size = 0;
	
	int n;
	int offset;
	int line;
	
	short x_size;
	short y_size;
	
	unsigned char texture_name[8];
	
	int bytes_to_go;
	int bytes_copied;
	int byte_count;
	
	
	
	output = (char *)malloc(1);
	
	
	
	for(i=0; i < 8; i++){
		texture_name[i] = table[(lump*16)+8+i];
		if(texture_name[i] >= 'A' && texture_name[i] <= 'Z')
			texture_name[i] += 0x20;
	}
	
	
	
	for(i=0; i < texture1_size; i++){
		if(texture1[i] == texture_name[0]){
			if(texture1[i+1] == texture_name[1]){
				if(texture1[i+2] == texture_name[2]){
					if(texture1[i+3] == texture_name[3]){
						if(texture1[i+4] == texture_name[4]){
							if(texture1[i+5] == texture_name[5]){
								if(texture1[i+6] == texture_name[6]){
									if(texture1[i+7] == texture_name[7]){
										// Width
										x_size = (texture1[i+13] << 8) | texture1[i+12];
										fwrite(&x_size, 2, 1, out_file);
										// Height
										y_size = (texture1[i+15] << 8) | texture1[i+14];
										fwrite(&y_size, 2, 1, out_file);
										
										i = texture1_size;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	// X offset
	i = (x_size>>1)-1;
	fwrite(&i, 2, 1, out_file);
	// Y offset
	i = 0x7B;
	fwrite(&i, 2, 1, out_file);
	
	line_lookup_size = x_size << 2;
	line_lookup = (char *)malloc(line_lookup_size);
	
	n = line_lookup_size + 8;
	for(i=0; i < x_size; i++){
		fwrite(&n, 4, 1, out_file);
		n += (y_size+5);
	}
	
	
	
	offset = 0;
	
	for(line=0; line < x_size; line++){
		fputc(0, out_file);
		fputc(y_size, out_file);
		
		
		
		// duplicate byte
		fputc(data[data_ptr + offset], out_file);

		fwrite(&data[data_ptr + offset], 1, y_size, out_file);
		
		// duplicate byte
		fputc(data[data_ptr + offset + y_size-1], out_file);
		
		
		
		fputc(0xFF, out_file);
		
		offset += y_size;
	}
	
	WriteTable(lump, out_file_size, ((y_size+5)*x_size) + 8 + line_lookup_size);
	
	out_file_size += (((y_size+5)*x_size) + 8 + line_lookup_size);
	
	
	
	free(line_lookup);
	free(output);
}



void ConvertSpriteData32X(int lump){
	int data_ptr = *(int *)&table[(lump*16)] - 0xC;
	int data_size = *(int *)&table[(lump*16)+4];
	
	short width;
	short height;
	short x_offset;
	short y_offset;
	
	int di = 0;		// data 'i'
	int ki = 0;		// key_data 'i'
	int ri = 0;		// raw_data 'i'
	
	int i = 0;
	int line = 0;
	
	int word = 0;
	int byte_count = 0;
	int bytes_copied = 0;
	
	unsigned char *key_table;
	unsigned char *key_data;
	unsigned char *raw_data;
	
	
	
	// Width
	width = (data[data_ptr+1]<<8) | data[data_ptr+0];
	fputc(data[data_ptr+1], out_file);
	fputc(data[data_ptr+0], out_file);
	// Height
	height = (data[data_ptr+3]<<8) | data[data_ptr+2];
	fputc(data[data_ptr+3], out_file);
	fputc(data[data_ptr+2], out_file);
	// X offset
	x_offset = (data[data_ptr+5]<<8) | data[data_ptr+4];
	fputc(data[data_ptr+5], out_file);
	fputc(data[data_ptr+4], out_file);
	// Y offset
	y_offset = (data[data_ptr+7]<<8) | data[data_ptr+6];
	fputc(data[data_ptr+7], out_file);
	fputc(data[data_ptr+6], out_file);
	
	
	
	key_table = (char *)malloc(width<<1);	// 32X table uses shorts as opposed to longs
	key_data = (char *)malloc(1);
	raw_data = (char *)malloc(1);
	
	while(line < width){
		word = data[data_ptr + (width<<2) + 8 + di];
		
		
		
		if(word != 0xFF){
			word <<= 8;
			word |= data[data_ptr + (width<<2) + 8 + di+1];
			di += 2;
		}
		else{
			word = 0xFFFF;
			di++;
		}
		
		
		
		if(word == 0xFFFF){
			line++;
			if(line < width){
				for(i=0; line < width && data[data_ptr + (width<<2) + 8 + di + i] == 0xFF; i++){
					line++;
				}
				di += i;
				
				i++;
				key_data = realloc(key_data, ki + (i<<1));
				while(i > 0){
					*(short *)&key_data[ki] = 0xFFFF;
					ki += 2;
					i--;
				}
			}
			else{
				key_data = realloc(key_data, ki + 2);
				*(short *)&key_data[ki] = 0xFFFF;
				ki += 2;
			}
		}
		else{
			key_data = realloc(key_data, ki+4);
			key_data[ki] = word>>8;
			key_data[ki+1] = word;
			
			*(short *)&key_data[ki+2] = swap_endian(bytes_copied+1)>>16;
			ki += 4;
			
			raw_data = realloc(raw_data, ri + (word & 0xFF) + 2);
			byte_count = (word & 0xFF) + 2;
			for(i=0; i < byte_count; i++){
				raw_data[ri] = data[data_ptr + (width<<2) + 8 + di];
				di++;
				ri++;
			}
			bytes_copied += i;
		}
	}
	
	while(ri & 3){
		ri++;
		raw_data = realloc(raw_data, ri);
		raw_data[ri-1] = 0;
	}
	
	
	
	*(short *)&key_table[0] = swap_endian((width<<1) + 8)>>16;
	
	i = 0;
	for(line=1; line < width; line++){
		while(*(unsigned short *)&key_data[i] != 0xFFFF)
			i += 2;
		
		*(short *)&key_table[line<<1] = swap_endian((width<<1) + 8 + i + 2)>>16;
		i += 2;
	}
	
	
	
	fwrite(key_table, 2, width, out_file);
	fwrite(key_data, 1, ki, out_file);
	WriteTable(lump, out_file_size, (width<<1) + ki + 8);
	out_file_size += ((width<<1) + ki + 8);
	
	fwrite(raw_data, 1, ri, out_file);
	WriteTableCustom(out_file_size, ri, ".");
	out_file_size += ri;
	
	
	
	free(key_table);
	free(key_data);
	free(raw_data);
}



void ConvertSpriteData(int lump){
	int key_ptr = *(int *)&table[(lump*16)] - 0xC;
	int key_size = *(int *)&table[(lump*16)+4];
	int data_ptr = *(int *)&table[(lump*16)+16] - 0xC;
	int data_size = *(int *)&table[(lump*16)+20];
	
	unsigned char *uncompressed;
	unsigned char *written_bytes;
	unsigned char *line_lookup;
	
	int line_lookup_size;
	
	int array_size = 0;
	short prev_array_size = 0;
	
	int word;
	short location;
	
	int bytes_to_go;
	int bytes_copied_key;
	int bytes_copied_data;
	int byte_count;
	
	
	
	uncompressed = (char *)malloc(1);
	written_bytes = (char *)malloc(1);
	
	
	
	// Width
	fputc(data[key_ptr+1], out_file);
	fputc(data[key_ptr+0], out_file);
	// Height
	fputc(data[key_ptr+3], out_file);
	fputc(data[key_ptr+2], out_file);
	// X offset
	fputc(data[key_ptr+5], out_file);
	fputc(data[key_ptr+4], out_file);
	// Y offset
	fputc(data[key_ptr+7], out_file);
	fputc(data[key_ptr+6], out_file);
	
	i = (data[key_ptr+8]<<8) | data[key_ptr+9];
	
	
	
	bytes_copied_key = 0;
	bytes_to_go = key_size - i;
	while(i < key_size-2){
		word = (data[key_ptr+i]<<8) | data[key_ptr+i+1];
		i += 2;
		if(word == 0xFFFF){
			// MAKE IT DETECT OTHER POSSIBLE INSTANCES OF 0xFFFF FIRST!!!
			if(i < key_size-2){
				word <<= 24;
				
				for(n=0; data[key_ptr+i] == 0xFF && data[key_ptr+i+1] == 0xFF; n++)
					i += 2;
				word |= (n<<16);
				word |= ((data[key_ptr+i]<<8) | data[key_ptr+i+1]);
				i += 2;
			}
		}
		
		location = ((data[key_ptr+i]<<8) | data[key_ptr+i+1]) - 1;
		i += 2;
		
		if(word < 0){
			prev_array_size = array_size;
			array_size = location+3+bytes_copied_key + ((word >> 16) & 0xFF);
			uncompressed = (char *)realloc(uncompressed, array_size);
			written_bytes = (char *)realloc(written_bytes, array_size);
			
			while(prev_array_size < array_size){
				written_bytes[prev_array_size] = 0;
				prev_array_size++;
			}
			
			for(n=0; n <= (unsigned char)((word >> 16) & 0xFF); n++){
				uncompressed[location + bytes_copied_key+n] = 0xFF;
				written_bytes[location + bytes_copied_key+n] = 1;
			}
			
			uncompressed[location + bytes_copied_key+n] = word >> 8;
			uncompressed[location + bytes_copied_key+n+1] = word;
			
			written_bytes[location + bytes_copied_key+n] = 1;
			written_bytes[location + bytes_copied_key+n+1] = 1;
			
			bytes_copied_key += (3 + ((word >> 16) & 0xFF));
		}
		else{
			prev_array_size = array_size;
			array_size = location+2+bytes_copied_key;
			uncompressed = (char *)realloc(uncompressed, array_size);
			written_bytes = (char *)realloc(written_bytes, array_size);
			
			while(prev_array_size < array_size){
				written_bytes[prev_array_size] = 0;
				prev_array_size++;
			}
			
			uncompressed[location + bytes_copied_key] = word >> 8;
			uncompressed[location + bytes_copied_key+1] = word;
			
			written_bytes[location + bytes_copied_key] = 1;
			written_bytes[location + bytes_copied_key+1] = 1;
			
			bytes_copied_key += 2;
		}
	}
	
	bytes_copied_data = 0;
	for(n=0; n < data_size; n++){
		if((n + bytes_copied_data) >= array_size){
			array_size++;
			
			uncompressed = (char *)realloc(uncompressed, array_size);
			written_bytes = (char *)realloc(written_bytes, array_size);
			
			written_bytes[n + bytes_copied_data] = 0;
		}
		if(written_bytes[n + bytes_copied_data]){
			bytes_copied_data++;
			n--;
		}
		else
			uncompressed[n + bytes_copied_data] = data[data_ptr+n];
	}
	
	
	
	n = 0;
	for(i=0; i < array_size; i++){
		if(uncompressed[i] == 0xFF)
			n++;
	}
	
	line_lookup_size = (n+1)<<2;
	line_lookup = (char *)malloc(line_lookup_size);
	
	
	n = 0;
	*(int *)&line_lookup[0] = line_lookup_size + 8;
	for(i=0; i < array_size; i++){
		if(uncompressed[i] == 0xFF){
			n++;
			*(int *)&line_lookup[n<<2] = line_lookup_size + 8 + i+1;
		}
	}
	
	fwrite(line_lookup, 1, line_lookup_size, out_file);
	
	
	
	i = (*(int *)&line_lookup[n<<2]) + 1 - line_lookup_size - 8;
	byte_count = uncompressed[i] + 4;
	uncompressed = (char *)realloc(uncompressed, array_size + 2);
	uncompressed[array_size] = 0;
	uncompressed[array_size+1] = 0;
	while(uncompressed[i-1] != 0 || uncompressed[i] != 0){
		i += byte_count;
		byte_count = uncompressed[i] + 4;
	}
	array_size = i;
	free(line_lookup);
	
	
	
	uncompressed = (char *)realloc(uncompressed, array_size);
	uncompressed[array_size-1] = 0xFF;
	
	while(array_size & 3){
		array_size++;
		uncompressed = (char *)realloc(uncompressed, array_size);
		uncompressed[array_size-1] = 0;
	}
	
	fwrite(uncompressed, 1, array_size, out_file);
	
	WriteTable(lump, out_file_size, array_size + line_lookup_size + 8);
	out_file_size += (array_size + line_lookup_size + 8);
	
	
	
	free(uncompressed);
	free(written_bytes);
}



void WritePatchName(char *name){
	char i;
	
	if(pnames_table == 0){
		patches_count = 1;
		pnames_table = (char *)malloc(12);
		*(int *)&pnames_table[0] = 1;
	}
	else{
		patches_count++;
		pnames_table = (char *)realloc(pnames_table, 4 + (patches_count<<3));
		*(int *)&pnames_table[0] = patches_count;
	}
	
	for(i=0; i<8; i++){
		if(name[i] >= 'a' && name[i] <= 'z')
			name[i] -= 0x20;
		
		if(name[i] == 0){
			while(i < 8){
				pnames_table[4+((patches_count-1)<<3)+i] = 0;
				i++;
			}
		}
		else{
			pnames_table[4+((patches_count-1)<<3)+i] = name[i];
		}
	}
}



void WriteTable(int lump, int ptr, int size){
	out_table = (char *)realloc(out_table, out_table_size + 16);
	
#ifndef FORCE_LITTLE_ENDIAN
	if(conversion_task == PC_2_MARS){
		ptr = swap_endian(ptr);
		size = swap_endian(size);
	}
#endif
	
	*(int *)&out_table[out_table_size] = ptr;
	*(int *)&out_table[out_table_size+4] = size;
	out_table[out_table_size+8] = table[(lump*16)+8];
	out_table[out_table_size+9] = table[(lump*16)+9];
	out_table[out_table_size+10] = table[(lump*16)+10];
	out_table[out_table_size+11] = table[(lump*16)+11];
	out_table[out_table_size+12] = table[(lump*16)+12];
	out_table[out_table_size+13] = table[(lump*16)+13];
	out_table[out_table_size+14] = table[(lump*16)+14];
	out_table[out_table_size+15] = table[(lump*16)+15];
	
	out_table_size += 16;
}



void WriteTableCustom(int ptr, int size, unsigned char *name){
	char i;
	
	out_table = (char *)realloc(out_table, out_table_size + 16);
	
#ifndef FORCE_LITTLE_ENDIAN
	if(conversion_task == PC_2_MARS){
		ptr = swap_endian(ptr);
		size = swap_endian(size);
	}
#endif
	
	*(int *)&out_table[out_table_size] = ptr;
	*(int *)&out_table[out_table_size+4] = size;
	for(i=0; i<8; i++){
		if(name[i] != 0)
			out_table[out_table_size+8+i] = name[i];
		else{
			while(i<8){
				out_table[out_table_size+8+i] = 0;
				i++;
			}
		}
	}
	
	out_table_size += 16;
}



void WriteTableStart(char type){
	out_table = (char *)realloc(out_table, out_table_size + 16);
	
	*(int *)&out_table[out_table_size] = 0;
	*(int *)&out_table[out_table_size+4] = 0;
	
	switch(type){
		case TYPE_TEXTURE:
			if(conversion_task == MARS_2_PC)
				out_table[out_table_size+8] = 'P';
			else	// PC_2_MARS
				out_table[out_table_size+8] = 'T';
			break;
		case TYPE_FLOOR:
			out_table[out_table_size+8] = 'F';
			break;
		case TYPE_SPRITE:
			out_table[out_table_size+8] = 'S';
			break;
	}
	out_table[out_table_size+9] = '_';
	out_table[out_table_size+10] = 'S';
	out_table[out_table_size+11] = 'T';
	out_table[out_table_size+12] = 'A';
	out_table[out_table_size+13] = 'R';
	out_table[out_table_size+14] = 'T';
	out_table[out_table_size+15] = 0;
	
	out_table_size += 16;
}



void WriteTableEnd(char type){
	out_table = (char *)realloc(out_table, out_table_size + 16);
	
	*(int *)&out_table[out_table_size] = 0;
	*(int *)&out_table[out_table_size+4] = 0;
	
	switch(type){
		case TYPE_TEXTURE:
			texture_end_written = 1;
			if(conversion_task == MARS_2_PC)
				out_table[out_table_size+8] = 'P';
			else	// PC_2_MARS
				out_table[out_table_size+8] = 'T';
			break;
		case TYPE_FLOOR:
			floor_end_written = 1;
			out_table[out_table_size+8] = 'F';
			break;
		case TYPE_SPRITE:
			sprite_end_written = 1;
			out_table[out_table_size+8] = 'S';
			break;
	}
	out_table[out_table_size+9] = '_';
	out_table[out_table_size+10] = 'E';
	out_table[out_table_size+11] = 'N';
	out_table[out_table_size+12] = 'D';
	out_table[out_table_size+13] = 0;
	out_table[out_table_size+14] = 0;
	out_table[out_table_size+15] = 0;
	
	out_table_size += 16;
}



void WriteTexture1(short x_size, short y_size, unsigned char *name){
	int i;
	
	num_of_textures++;
	
	if(out_texture1_data){
		out_texture1_data = (char *)realloc(out_texture1_data, num_of_textures << 5);
		out_texture1_table = (char *)realloc(out_texture1_table, (num_of_textures << 2) + 4);
	}
	else{
		out_texture1_data = (char *)malloc(num_of_textures << 5);
		out_texture1_table = (char *)malloc((num_of_textures << 2) + 4);
	}
	
	*(int *)&out_texture1_table[0] = num_of_textures;
	for(i=0; i < num_of_textures; i++){
		*(int *)&out_texture1_table[(i<<2) + 4] = (num_of_textures << 2) + (i << 5) + 4;
	}
	
	for(i=0; i<8; i++){
		out_texture1_data[(num_of_textures << 5) - 32 + i] = 0;
		if(name[i] >= 'A' && name[i] <= 'Z')
			name[i] += 0x20;	// we do this just to keep with convention
	}
	strcpy(&out_texture1_data[(num_of_textures << 5) - 32], name);
	*(int *)&out_texture1_data[(num_of_textures << 5) - 24] = 0;
	*(short *)&out_texture1_data[(num_of_textures << 5) - 20] = x_size;
	*(short *)&out_texture1_data[(num_of_textures << 5) - 18] = y_size;
	*(int *)&out_texture1_data[(num_of_textures << 5) - 16] = 0;
	*(short *)&out_texture1_data[(num_of_textures << 5) - 12] = 1;
	*(short *)&out_texture1_data[(num_of_textures << 5) - 10] = 0;
	*(short *)&out_texture1_data[(num_of_textures << 5) - 8] = 0;
	//*(short *)&out_texture1_data[(num_of_textures << 5) - 6] = num_of_textures-1;
	*(short *)&out_texture1_data[(num_of_textures << 5) - 4] = 1;
	*(short *)&out_texture1_data[(num_of_textures << 5) - 2] = 0;
}


int swap_endian(unsigned int i){
	return (i>>24) + ((i>>8)&0x0000FF00) + ((i<<8)&0x00FF0000) + (i<<24);
}
