/***************************************************************

   The Subread software package is free software package: 
   you can redistribute it and/or modify it under the terms
   of the GNU General Public License as published by the 
   Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   Subread is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   
   See the GNU General Public License for more details.

   Authors: Drs Yang Liao and Wei Shi

  ***************************************************************/
  
  
#ifndef __HELPER_FUNCTIONS_H_
#define __HELPER_FUNCTIONS_H_

#include "subread.h"
#include "hashtable.h"

#define PARSE_STATUS_TAGNAME 1
#define PARSE_STATUS_TAGTYPE 2
#define PARSE_STATUS_TAGVALUE 3

typedef struct{
	HashTable * contig_table;
	HashTable * size_table;
} fasta_contigs_t;

#ifndef MAKE_STANDALONE

typedef struct{
	ArrayList * message_queue;
	subread_lock_t queue_lock;
	subread_lock_t queue_notifier;
	int is_finished;
} message_queue_t;

extern message_queue_t mt_message_queue;
#endif

void msgqu_init();
void msgqu_destroy();
void msgqu_main_loop();
void msgqu_notifyFinish();
void msgqu_printf(const char * fmt, ...);

int read_contig_fasta(fasta_contigs_t * tab, char * fname);
int get_contig_fasta(fasta_contigs_t * tab, char * chro, unsigned int pos, int len, char * out_bases);
void destroy_contig_fasta(fasta_contigs_t * tab);

// This function parses CIGAR_Str and extract the relative starting points and lengths of all sections (i.e., the sections of read that are separated by 'N').
// CIGAR_Str is a CIGAR string containing 'S', 'M', 'I', 'D' and 'N' operations. Other operations are all ignored. The length of CIGAR_Str should be always less than 100 bytes or "-1" is returned.
// Staring_Points and Section_Length are empty arrays to write the sections. The minimum length of each array is 6 items.
// The length of a section is its length on the chromosome, namely 'I' is ignored but 'D' is added into the length.
// This function ignores all sections from the 7-th.

// This function returns the number of sections found in the CIGAR string. It returns -1 if the CIGAR string cannot be parsed.

int RSubread_parse_CIGAR_string(char * chro , unsigned int first_pos, const char * CIGAR_Str, int max_M, char ** Section_Chromosomes, unsigned int * Section_Start_Chro_Pos,unsigned short * Section_Start_Read_Pos, unsigned short * Section_Chro_Length, int * is_junction_read);


int RSubread_parse_CIGAR_Extra_string(int FLAG, char * MainChro, unsigned int MainPos, const char * CIGAR_Str, const char * Extra_Tags, int max_M, char ** Chros, unsigned int * Staring_Chro_Points, unsigned short * Section_Start_Read_Pos, unsigned short * Section_Length, int * is_junction_read);

// This function try to find the attribute value of a given attribute name from the extra column string in GTF/GFF.
// If the value is found, it returns the length of the value (must be > 0 by definition), or -1 if no attribute is found or the format is wrong.

int GTF_extra_column_value(const char * Extra_Col, const char * Target_Name, char * Target_Value, int TargVal_Size);


// Replacing `rep' with `with' in `orig'. 
// Rhe return value must be freed if it is not NULL.
char *str_replace(char *orig, char *rep, char *with) ;


// rule: the string is ABC123XXXXXX...
// // This is the priroity:
// // First, compare the letters part.
// // Second, compare the pure numeric part.
// // Third, compare the remainder.
int strcmp_number(char * s1, char * s2);

unsigned int reverse_cigar(unsigned int pos, char * cigar, char * new_cigar);
unsigned int find_left_end_cigar(unsigned int right_pos, char * cigar);
int mac_or_rand_str(char * char_14);

double fast_fisher_test_one_side(unsigned int a, unsigned int b, unsigned int c, unsigned int d, long double * frac_buffer, int buffer_size);
int load_features_annotation(char * file_name, int file_type, char * gene_id_column, char * transcript_id_column, char * used_feature_type,
 void * context, int do_add_feature(char * gene_name, char * transcript_id, char * chrome_name, unsigned int start, unsigned int end, int is_negative_strand, void * context)  );

HashTable * load_alias_table(char * fname) ;

char * get_short_fname(char * lname);

// Rebuild a string containing the command line.
// Return the string length (without the terminating \0)
// You need to free(*lineptr) after all.
int rebuild_command_line(char ** lineptr, int argc, char ** argv);


// Calculate a full round of MD5 or SHA256. 
void Helper_md5sum(char * plain_txt, int plain_len, unsigned char * bin_md5_buff);

typedef unsigned int HelpFuncMD5_u32plus;

typedef struct {
	HelpFuncMD5_u32plus lo, hi;
	HelpFuncMD5_u32plus a, b, c, d;
	unsigned char buffer[64];
	HelpFuncMD5_u32plus block[16];
} HelpFuncMD5_CTX;
 

void HelpFuncMD5_Init(HelpFuncMD5_CTX *ctx);
void HelpFuncMD5_Update(HelpFuncMD5_CTX *ctx, const void *data, unsigned long size);
void HelpFuncMD5_Final(unsigned char *result, HelpFuncMD5_CTX *ctx);


void Helper_sha256sum(char * plain_txt, int plain_len, unsigned char * bin_md5_buff);
unsigned long long plain_txt_to_long_rand(char * plain_txt, int plain_len);

// give me a p, I give you the value such that Pr( x < value ) == p in a 0/1 normal distribution.
double inverse_sample_normal(double p);
#endif
