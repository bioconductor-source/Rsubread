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
  
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "subread.h"
#include "gene-algorithms.h"
#include "sambam-file.h"

BS_uint_16 gzread_B16(gzFile fp)
{
	BS_uint_16 ret;
	gzread(fp, &ret, 2);
	return ret;
}

BS_uint_32 gzread_B32(gzFile fp)
{
	BS_uint_32 ret;
	gzread(fp, &ret, 4);
	return ret;
}

BS_uint_8 gzread_B8(gzFile fp)
{
	BS_uint_8 ret;
	gzread(fp, &ret, 1);
	return ret;
}


SamBam_FILE * SamBam_fopen(char * fname , int file_type)
{
	SamBam_FILE * ret = (SamBam_FILE *)malloc(sizeof(SamBam_FILE));
	ret -> file_type = file_type;

	if(file_type ==SAMBAM_FILE_SAM) 
	{
		ret -> os_file = fopen(fname, "rb");
		if(!ret -> os_file)
		{
			free(ret);
			return NULL;
		}
		fseek(ret -> os_file,0,SEEK_SET);
	}
	else
	{
		FILE * os_file = fopen(fname, "rb");
		if(os_file == NULL)
		{
			free(ret);
			return NULL;
		}
		unsigned char first_ch = fgetc(os_file);
		unsigned char second_ch = fgetc(os_file);

		fclose(os_file);
		if(first_ch!=31 || second_ch!=139)
		{
			free(ret);
			return NULL;
		}


		gzFile nf = gzopen(fname, "rb");
		if(!nf)
		{
			free(ret);
			return NULL;
		}

		ret -> gz_file = nf;
		ret -> bam_file_stage = BAM_FILE_STAGE_HEADER;
		
		BS_uint_32 magic_4 = gzread_B32(ret -> gz_file);
		if(magic_4 != 21840194) // this number is the four bytes of "BAM1"
		{
			free(ret);
			return NULL;
		}
		BS_uint_32 l_text = gzread_B32(ret -> gz_file);
		ret -> bam_file_next_section_start = gztell(ret -> gz_file) + l_text;
	}
	return ret;
}

char cigar_op_char(int ch)
{
	assert(ch<9);
	return "MIDNSHP=X"[ch];
}

char read_int_char(int ch)
{
	assert(ch<16);
	return "=ACMGRSVTWYHKDBN"[ch];
}

int SamBam_get_alignment(SamBam_FILE * fp, SamBam_Alignment * aln, int seq_needed)
{
		if(gzeof(fp->gz_file)) return -1;
		unsigned long long head_pos = gztell(fp->gz_file);
		unsigned int block_size = gzread_B32(fp->gz_file);
		if(gzeof(fp->gz_file)) return -1;

		unsigned int ref_id = gzread_B32(fp->gz_file);
		if(gzeof(fp->gz_file)) return -1;

		assert(ref_id < fp->bam_chro_table_size|| ref_id == -1);


		if(ref_id == -1) aln -> chro_name = NULL;
		else aln -> chro_name = fp -> bam_chro_table[ref_id].chro_name; 
		aln -> chro_offset = gzread_B32(fp->gz_file);

		unsigned int comb1 = gzread_B32(fp->gz_file);
		aln -> mapping_quality = 0xff & (comb1 >> 8);

		unsigned int comb2 = gzread_B32(fp->gz_file);
		aln -> flags = 0xffff&(comb2 >> 16);

		unsigned int read_len = gzread_B32(fp->gz_file);

		unsigned int mate_ref_id = gzread_B32(fp->gz_file);
		if(gzeof(fp->gz_file)) return -1;

		assert(mate_ref_id < fp->bam_chro_table_size || mate_ref_id == -1);
		if(mate_ref_id == -1) aln -> mate_chro_name = NULL;
		else aln -> mate_chro_name = fp -> bam_chro_table[mate_ref_id].chro_name; 

		aln -> mate_chro_offset = gzread_B32(fp->gz_file);

		aln -> templete_length = (int)gzread_B32(fp->gz_file);

		int read_name_len = comb1 & 0xff;
		assert(read_name_len < BAM_MAX_READ_NAME_LEN);

		gzread(fp->gz_file, aln -> read_name, read_name_len);
		aln -> read_name[read_name_len] = 0;

		int cigar_ops = comb2 & 0xffff;
		int xk1;
		aln -> cigar[0]=0; 
		for(xk1=0; xk1<cigar_ops;xk1++)
		{
			char cigar_piece_buf[BAM_MAX_CIGAR_LEN];
			unsigned int cigar_piece = gzread_B32(fp->gz_file);

			sprintf(cigar_piece_buf, "%u%c", cigar_piece>>4, cigar_op_char(cigar_piece&0xf));
			if(strlen(cigar_piece_buf)+strlen(aln->cigar)<BAM_MAX_CIGAR_LEN-1)
				strcat(aln->cigar, cigar_piece_buf);
			else
				SUBREADprintf("WARNING: cigar string is too long to the buffer\nThis can happen when the BAM file was generated by a very old version of SAMTools.\n");
		}

		char read_2_seq = 0;
		int seq_qual_bytes = read_len + (read_len /2)+(read_len%2);
		int gzread_len = gzread(fp->gz_file, aln-> buff_for_seq, seq_qual_bytes);
		if(gzread_len < seq_qual_bytes)
			return -1;

		if(seq_needed)
		{
			for(xk1=0;xk1<read_len;xk1++)
			{
				if(xk1 %2 == 0){
					read_2_seq = aln-> buff_for_seq[xk1/2];
				}
				if(xk1 < BAM_MAX_READ_LEN)
					aln -> sequence[xk1] = read_int_char(0xf&(read_2_seq >> (xk1%2?0:4)));
			}
			aln -> sequence[min(BAM_MAX_READ_LEN-1,read_len)] = 0;
			if(read_len >= BAM_MAX_READ_LEN-1)
				SUBREADprintf("WARNING: read is too long to the buffer\n");

			
			for(xk1=0;xk1<read_len;xk1++)
			{
				read_2_seq = aln -> buff_for_seq[(read_len /2)+(read_len%2) + xk1] ;
				if(xk1 < BAM_MAX_READ_LEN)
					aln -> seq_quality[xk1] = 33+read_2_seq;
			}
			aln -> seq_quality[min(BAM_MAX_READ_LEN-1,read_len)] = 0;
		}
		else
		{
			aln -> seq_quality[0]='#';
			aln -> seq_quality[1]=0;
			aln -> sequence[0]='N';
			aln -> sequence[1]=0;
		}

		unsigned long long int skip_len=0;
		aln -> NH_number = -1;
		while(gztell(fp->gz_file) < head_pos+block_size+4)
		{
			char tagname[2];
			gzread(fp->gz_file, tagname, 2);
			char tagtype, nch;
			gzread(fp->gz_file, &tagtype, 1);
			skip_len=9999999;


			switch(tagtype)
			{
				case 'Z':	// string
					while(1)
					{
						gzread(fp->gz_file, &nch,1);
						if(nch==0)break;
					}
					skip_len=0;
					break;
				case 'c':
				case 'C':
					skip_len=1;
					break;
				case 's':
				case 'S':
					skip_len=2;
					break;
				case 'i':
				case 'I':
					skip_len=4;
					break;
				default: break;
			}
			//printf("TG=%s\tTY=%c\tSKP=%d\n", tagname, tagtype, skip_len);

			if(skip_len>9999998) break;

			if(memcmp(tagname,"NH",2)==0)
			{
				int nh_v=0;
				gzread(fp->gz_file, &nh_v, skip_len);
				aln -> NH_number = nh_v;
			}
			else
				gzseek(fp->gz_file, skip_len, SEEK_CUR);
		}

		unsigned long long tail_pos = gztell(fp->gz_file);
		skip_len = block_size - (tail_pos - head_pos - 4);
		long long int seek_ret= gzseek(fp->gz_file, skip_len, SEEK_CUR);
		if(seek_ret < 0) return -1;
		return 0;
}

void SamBam_fclose(SamBam_FILE * fp)
{
	if(fp->file_type==SAMBAM_FILE_SAM)
	{
		fclose(fp->os_file);
		free(fp);
	}
	else
	{
		gzclose(fp->gz_file);
		free(fp -> bam_chro_table);
		free(fp);
	}
}

int SamBam_feof(SamBam_FILE * fp)
{
	if(fp->file_type==SAMBAM_FILE_SAM) return feof(fp->os_file);
	else{
			if(gzeof(fp->gz_file)) return 1;
			return 0;
	}
}

void SamBam_read_ref_info(SamBam_FILE * ret)
{
	unsigned int ref_info_size = gzread_B32(ret -> gz_file);

	int xk1;
	ret -> bam_chro_table = malloc(sizeof(SamBam_Reference_Info) * ref_info_size);
	for(xk1=0;xk1<ref_info_size;xk1++)
	{
		int ref_name_len = gzread_B32(ret -> gz_file);
		int ref_readin_len = min(ref_name_len, BAM_MAX_CHROMOSOME_NAME_LEN-1);
		int ref_skip_len = ref_name_len - ref_readin_len;

		gzread(ret -> gz_file, ret -> bam_chro_table[xk1].chro_name , ref_readin_len);
		ret -> bam_chro_table[xk1].chro_name[ref_readin_len] = 0;
		if(ref_skip_len)gzseek(ret -> gz_file , ref_skip_len , SEEK_CUR);


		ret -> bam_chro_table[xk1].chro_length = gzread_B32(ret -> gz_file);

		//SUBREADprintf("CHRO[%d] : %s [%d]\n", xk1+1, ret -> bam_chro_table[xk1].chro_name , ret -> bam_chro_table[xk1].chro_length);
	}
	ret ->bam_chro_table_size = ref_info_size;
}

char * SamBam_fgets(SamBam_FILE * fp, char * buff , int buff_len, int seq_needed)
{
	if(fp->file_type==SAMBAM_FILE_SAM){
		char * ret = fgets(buff, buff_len, fp->os_file);
		if(strlen(buff)<1) return NULL;
		else return ret;
	}
	else
	{
		int xk1;
		// decrypt the BAM mess.
		if(fp-> bam_file_stage == BAM_FILE_STAGE_HEADER)
		{
			char nch;
			xk1=0;
			while(1)
			{
				if(xk1 >= buff_len-2 || gztell(fp->gz_file) >= fp -> bam_file_next_section_start)
					break;

				nch = gzgetc(fp->gz_file);
				if(nch == '\r'||nch=='\n' || nch <0) break;
				buff[xk1]=nch;
				xk1++;
			}

			if(xk1<buff_len-1){
				buff[xk1]='\n';
				buff[xk1+1]=0;
			}
			if(gztell(fp->gz_file) >= fp -> bam_file_next_section_start)
			{
				SamBam_read_ref_info(fp);
				fp -> bam_file_stage = BAM_FILE_STAGE_ALIGNMENT;
			}
			return buff;
		}
		else
		{
			SamBam_Alignment *aln = &fp->aln_buff;
			int is_align_error =SamBam_get_alignment(fp, aln, seq_needed);
			if(is_align_error)return NULL;
			else
			{
					char * chro_name = "*";
					char * cigar = "*";
					unsigned int chro_offset = 0;

					if(aln -> chro_name){
						chro_name = aln -> chro_name;
						chro_offset = aln -> chro_offset+1;
						if(aln -> cigar[0])
							cigar = aln -> cigar;
					}

					char * mate_chro_name = "*";
					unsigned int mate_chro_offset = 0;
					if(aln -> mate_chro_name)
					{
						if(aln -> mate_chro_name == chro_name) mate_chro_name = "=";
						else
							mate_chro_name = aln -> mate_chro_name;
						mate_chro_offset = aln -> mate_chro_offset+1;
					}

					long long int templete_length = aln -> templete_length;

					char ext_fields[20];
					if(aln->NH_number >=0)
						sprintf(ext_fields,"\tNH:i:%d", aln->NH_number);
					else	ext_fields[0]=0;
					
					snprintf(buff, buff_len-1, "%s\t%u\t%s\t%u\t%d\t%s\t%s\t%u\t%lld\t%s\t%s%s\n", aln -> read_name, aln -> flags , chro_name, chro_offset, aln -> mapping_quality, cigar, mate_chro_name, mate_chro_offset, templete_length, aln -> sequence , aln -> seq_quality, ext_fields);
			}
		
			return buff;
		}
	}
}



int PBam_get_next_zchunk(FILE * bam_fp, char * buffer, int buffer_length, unsigned int * real_len)
{
	unsigned char ID1, ID2, CM, FLG;
	unsigned short XLEN;
	int BSIZE=-1;

	if(feof(bam_fp)) return -1;

	fread(&ID1, 1, 1, bam_fp);
	fread(&ID2, 1, 1, bam_fp);
	fread(&CM, 1, 1, bam_fp);
	fread(&FLG, 1, 1, bam_fp);
	if(feof(bam_fp)) return -1;

	if(ID1!=31 || ID2!=139 || CM!=8 || FLG!=4)
	{
		//SUBREADprintf("4CHR = %d, %d, %d, %d\n", ID1, ID2, CM, FLG);
		return -1;
	}
	fseeko(bam_fp, 6, SEEK_CUR);
	fread(&XLEN,1, 2, bam_fp );

	int XLEN_READ = 0;
	while(1)
	{
		unsigned char SI1, SI2;
		unsigned short SLEN, BSIZE_MID;
		
		fread(&SI1, 1, 1, bam_fp);
		fread(&SI2, 1, 1, bam_fp);

		
		fread(&SLEN, 1, 2, bam_fp);
		if(SI1==66 && SI2== 67 && SLEN == 2)
		{
			fread(&BSIZE_MID, 1,2 , bam_fp);
			BSIZE = BSIZE_MID;
		}
		else	fseeko(bam_fp, SLEN, SEEK_CUR);
		XLEN_READ += SLEN + 4;
		if(XLEN_READ>=XLEN) break;
	}

	if(BSIZE>19)
	{
		int CDATA_LEN = BSIZE - XLEN - 19;
		int CDATA_READING = min(CDATA_LEN, buffer_length);
		fread(buffer, 1, CDATA_READING, bam_fp);
		if(CDATA_READING<CDATA_LEN)
			fseeko(bam_fp, CDATA_LEN-CDATA_READING, SEEK_CUR);
		fseeko(bam_fp, 4, SEEK_CUR);
		fread(&real_len, 4, 1, bam_fp);

		return CDATA_READING;
	}
	else
		return -1;
}


// returns 0 if the header finished.
// returns 1 if the header is going on.
// returns -1 if error.
int PBam_chunk_headers(char * chunk, int *chunk_ptr, int chunk_len, SamBam_Reference_Info ** bam_chro_table, int * table_size, int * table_items, int * state, int * header_txt_remainder, int * reminder_byte_len)
{

	if((*state)  == 0)
	{
		unsigned int header_txt_len ;
		if(0!=memcmp("BAM\x1",chunk + (*chunk_ptr),4))
			return -1;
		(*chunk_ptr)+=4;	// MAGIC
		(*state) = 1;

		memcpy(&header_txt_len, chunk + (*chunk_ptr),4);
		(*chunk_ptr)+=4;	
		if(header_txt_len + 8 < chunk_len)
		{
			(* state) = 2;
			(*chunk_ptr) += header_txt_len;
		}
		else
		{
			(* state) = 1;
			(* header_txt_remainder) = header_txt_len - (chunk_len - 8); 
			return 1;
		} 
	}

	if((*state) == 1)
	{
		if((*header_txt_remainder)<chunk_len)
		{
			(*state) = 2;
			(*chunk_ptr) += (*header_txt_remainder);
		}
		else if((*header_txt_remainder)==chunk_len)
		{
			(*state) = 2;
			return 1;
		}
		else	
		{
			(* header_txt_remainder) -= (chunk_len);
			return 1;
		}
	}

	if((*state) == 2 || (*state == 3))
	{
		int chrs, remainder_chrs;
		if((*state)==2)
		{
			memcpy(&chrs, chunk + (*chunk_ptr),4); 
			(*chunk_ptr)+=4;

			remainder_chrs = chrs;
		}
		else	remainder_chrs = (*header_txt_remainder);

		while((*chunk_ptr) < chunk_len && remainder_chrs>0)
		{
			int chro_name_len;
			unsigned int chro_len;
			(*reminder_byte_len) = chunk_len - (*chunk_ptr);

			if( (*chunk_ptr) < chunk_len-4)
			{
				memcpy(&chro_name_len, chunk + (*chunk_ptr),4);
				(*chunk_ptr)+=4;
				if( (*chunk_ptr) <= chunk_len-chro_name_len-4)
				{
					char * chro_name = chunk + (*chunk_ptr);
					(*chunk_ptr)+=chro_name_len;
					memcpy(&chro_len, chunk + (*chunk_ptr),4);
					(*chunk_ptr)+=4;

					(*reminder_byte_len) =0;

					//todo: insert item
					if(0==(* table_items))
					{
						(*table_size) = 50;
						(*bam_chro_table) = malloc(sizeof(SamBam_Reference_Info)*50);
					}
					else if((*table_size) <= (* table_items))
					{
						(*table_size) *= 2;
						(*bam_chro_table) = realloc((*bam_chro_table),sizeof(SamBam_Reference_Info)*(*table_size));
					}

					SamBam_Reference_Info * new_event = (*bam_chro_table) + (* table_items);
					strncpy(new_event->chro_name, chro_name, BAM_MAX_CHROMOSOME_NAME_LEN);
					new_event -> chro_length = chro_len;

					(* table_items)++;
					//SUBREADprintf("CHRO %d/%d added\n", (* table_items),(remainder_chrs));
					remainder_chrs --;
				}
				else break;
			}
			else break;

		}

		if(remainder_chrs)
		{
			(*state) = 3;
			(*header_txt_remainder) = remainder_chrs;
			return 1;
		}
		else{
			(*state) = 4;
			return 0;
		}
	}
	return -1;
}

int PBam_chunk_gets(char * chunk, int *chunk_ptr, SamBam_Reference_Info * bam_chro_table, char * buff , int buff_len, SamBam_Alignment*aln, int seq_needed)
{
	int xk1;
	// decrypt the BAM mess.
	unsigned int block_size;
	memcpy(&block_size, chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;
	unsigned int next_start = block_size+(*chunk_ptr);

	int ref_id;
	memcpy(&ref_id, chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;

	if(ref_id == -1) aln -> chro_name = NULL;
	else aln -> chro_name = bam_chro_table[ref_id].chro_name; 

	memcpy(&(aln -> chro_offset), chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;

	unsigned int comb1;
	memcpy(&comb1, chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;
	aln -> mapping_quality = 0xff & (comb1 >> 8);

	unsigned int comb2;
	memcpy(&comb2, chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;
	aln -> flags = 0xffff&(comb2 >> 16);

	unsigned int read_len;
	memcpy(&read_len, chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;

	unsigned int mate_ref_id;
	memcpy(&mate_ref_id, chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;

	if(mate_ref_id == -1) aln -> mate_chro_name = NULL;
	else aln -> mate_chro_name = bam_chro_table[mate_ref_id].chro_name; 

	memcpy(&(aln -> mate_chro_offset), chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;

	memcpy(&(aln -> templete_length), chunk+(*chunk_ptr), 4);
	(*chunk_ptr)+=4;

	int read_name_len = comb1 & 0xff;
	assert(read_name_len < BAM_MAX_READ_NAME_LEN);

	memcpy(aln -> read_name, chunk+(*chunk_ptr), read_name_len);
	aln -> read_name[read_name_len] = 0;
	(*chunk_ptr)+=read_name_len;

	int cigar_ops = comb2 & 0xffff;
	aln -> cigar[0]=0; 
	for(xk1=0; xk1<cigar_ops;xk1++)
	{
		char cigar_piece_buf[BAM_MAX_CIGAR_LEN];
		unsigned int cigar_piece;
		memcpy(&cigar_piece,  chunk+(*chunk_ptr),4);
		(*chunk_ptr)+=4;

		sprintf(cigar_piece_buf, "%u%c", cigar_piece>>4, cigar_op_char(cigar_piece&0xf));
		if(strlen(cigar_piece_buf)+strlen(aln->cigar)<BAM_MAX_CIGAR_LEN-1)
			strcat(aln->cigar, cigar_piece_buf);
		else
			SUBREADprintf("WARNING: cigar string is too long to the buffer\n");
	}

	char read_2_seq = 0;
	int seq_qual_bytes = read_len + (read_len /2)+(read_len%2);
	memcpy( aln-> buff_for_seq, chunk+(*chunk_ptr), seq_qual_bytes);

	(*chunk_ptr) += seq_qual_bytes;

	int nh_val = -1;
	while( (*chunk_ptr) < next_start)
	{
		char extag[2];
		char extype;
		int delta;
		memcpy(extag,  chunk+(*chunk_ptr), 2);
		extype = chunk[2+(*chunk_ptr)];
		(*chunk_ptr)+=3;
		if(extype == 'Z')
		{
			delta = 0;
			// 'Z' columns are NULL-terminated.
			while(chunk[*chunk_ptr]) (*chunk_ptr)++;
			(*chunk_ptr)++;
		}
		else if(extype == 'A') delta=1;
		else if(extype == 'c' || extype=='C') delta=1;
		else if(extype == 'i' || extype=='I' || extype == 'f') delta=4;
		else if(extype == 's' || extype=='S') delta=2;
		else if(extype == 'B') 
		{
			extype = (*chunk_ptr);
			(*chunk_ptr)++;
			if(extype == 'A' || extype=='Z') delta=1;
			else if(extype == 'c' || extype=='C') delta=1;
			else if(extype == 'i' || extype=='I' || extype == 'f') delta=4;
			else if(extype == 's' || extype=='S') delta=2;
			else break;

			int array_len;
			memcpy(&array_len, chunk+(*chunk_ptr), 4);
			(*chunk_ptr)+=4;
			delta *= array_len;
		}
		else break;
		
		if(memcmp(extag,"NH",2)==0 && delta<=4)
		{
			nh_val=0;
			memcpy(&nh_val, chunk+(*chunk_ptr),delta);
		}
		(*chunk_ptr)+=delta;
		
	}

	(*chunk_ptr) = next_start;

	if(seq_needed)
	{
		for(xk1=0;xk1<read_len;xk1++)
		{
			if(xk1 %2 == 0){
				read_2_seq = aln-> buff_for_seq[xk1/2];
			}
			if(xk1 < BAM_MAX_READ_LEN)
				aln -> sequence[xk1] = read_int_char(0xf&(read_2_seq >> (xk1%2?0:4)));
		}
		aln -> sequence[min(BAM_MAX_READ_LEN-1,read_len)] = 0;
		if(read_len >= BAM_MAX_READ_LEN-1)
			SUBREADprintf("WARNING: read is too long to the buffer\n");

		
		for(xk1=0;xk1<read_len;xk1++)
		{
			read_2_seq = aln -> buff_for_seq[(read_len /2)+(read_len%2) + xk1] ;
			if(xk1 < BAM_MAX_READ_LEN)
				aln -> seq_quality[xk1] = 33+read_2_seq;
		}
		aln -> seq_quality[min(BAM_MAX_READ_LEN-1,read_len)] = 0;
	}
	else
	{
		aln -> sequence[0]='N';
		aln -> sequence[1]=0;
		aln -> seq_quality[0]='#';
		aln -> seq_quality[1]=0;
	}

	char * chro_name = "*";
	char * cigar = "*";
	unsigned int chro_offset = 0;

	if(aln -> chro_name){
		chro_name = aln -> chro_name;
		chro_offset = aln -> chro_offset+1;
		if(aln -> cigar[0])
			cigar = aln -> cigar;
	}

	char * mate_chro_name = "*";
	unsigned int mate_chro_offset = 0;
	if(aln -> mate_chro_name)
	{
		if(aln -> mate_chro_name == chro_name) mate_chro_name = "=";
		else
			mate_chro_name = aln -> mate_chro_name;
		mate_chro_offset = aln -> mate_chro_offset+1;
	}

	long long int templete_length = aln -> templete_length;

	char nh_tag [20];
	nh_tag[0]=0;
	if(nh_val>=0)
		sprintf(nh_tag, "\tNH:i:%d",nh_val);

	int plen = snprintf(buff, buff_len-1, "%s\t%u\t%s\t%u\t%d\t%s\t%s\t%u\t%lld\t%s\t%s%s\n", aln -> read_name, aln -> flags , chro_name, chro_offset, aln -> mapping_quality, cigar, mate_chro_name, mate_chro_offset, templete_length, aln -> sequence , aln -> seq_quality, nh_tag);

	return plen;
}


int PBum_load_header(FILE * bam_fp, SamBam_Reference_Info** chro_tab)
{
	char * CDATA = malloc(80010);
	char * PDATA = malloc(1000000);

	int chro_tab_size = 0, chro_tab_items = 0, chro_tab_state = 0, header_remainder = 0, remainder_byte_len = 0; 
	z_stream strm;
	while(1)
	{
		unsigned int real_len = 0;
		int rlen = PBam_get_next_zchunk(bam_fp,CDATA,80000, & real_len);
		if(rlen<0) break;

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		int ret = inflateInit2(&strm, SAMBAM_GZIP_WINDOW_BITS);
		if (ret != Z_OK)
		{
			free(CDATA);
			free(PDATA);
			return -1;
		}
		strm.avail_in = (unsigned int)rlen;
		strm.next_in = (unsigned char *)CDATA;


		strm.avail_out = 1000000 - remainder_byte_len;
		strm.next_out = (unsigned char *)(PDATA + remainder_byte_len);
		ret = inflate(&strm, Z_FINISH);
		int have = 1000000 - strm.avail_out;
		int PDATA_ptr=0;

		inflateEnd(&strm);

		ret = PBam_chunk_headers(PDATA, &PDATA_ptr, have, chro_tab, &chro_tab_size, &chro_tab_items, &chro_tab_state, &header_remainder,&remainder_byte_len);
		memcpy(PDATA , PDATA + have - remainder_byte_len, remainder_byte_len);
		if(ret<0)
		{
			SUBREADprintf("Header error!\n");
			free(CDATA);
			free(PDATA);
			return -1;
		}
		else if(ret == 0)
		{
			//SUBREADprintf("Header loaded = %d\n", (chro_tab_items));
			remainder_byte_len=0;
		}
		if(chro_tab_state>3) break;
	}
	free(CDATA);
	free(PDATA);
	return 0;
}


int test_pbam(char * fname)
{
	FILE * bam_fp = fopen(fname, "rb");
	char * CDATA = malloc(80010);
	char * PDATA = malloc(1000000);

	z_stream strm;
	SamBam_Reference_Info * chro_tab;

	PBum_load_header(bam_fp, & chro_tab);

	while(1)
	{
		unsigned int real_len = 0;
		int rlen = PBam_get_next_zchunk(bam_fp,CDATA,80000, & real_len);
		if(rlen<0) break;

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		int ret = inflateInit2(&strm, SAMBAM_GZIP_WINDOW_BITS);
		if (ret != Z_OK)SUBREADprintf("Ohh!\n");

		strm.avail_in = (unsigned int)rlen;
		strm.next_in = (unsigned char *)CDATA;


		strm.avail_out = 1000000;
		strm.next_out = (unsigned char *)PDATA;
		ret = inflate(&strm, Z_FINISH);
		int have = 1000000 - strm.avail_out;
		inflateEnd(&strm);

		int PDATA_ptr=0;

		while(PDATA_ptr < have)
		{
			char * read_line = malloc(3000);
			SamBam_Alignment  aln;
			PBam_chunk_gets(PDATA, &PDATA_ptr, chro_tab, read_line , 2999, &aln, 0);
			SUBREADprintf("%s", read_line);
			free(read_line);
		}
	}
	free(CDATA);
	free(PDATA);
	fclose(bam_fp);

	return 0;
}
int test_bamview(int argc, char ** argv)
{
	if(argc>1)
	{
		SamBam_FILE * fp = SamBam_fopen(argv[1], SAMBAM_FILE_BAM);
		assert(fp);
		/*
		while(1)
		{
			char buf[3000];
			char * buf2 = SamBam_fgets(fp,buf, 3000);
			//SUBREADprintf(">>%s<<\n",buf);
			//if(buf2)
			//	fwrite(buf,strlen(buf), 1, stdout);
			//else break;
		}
		*/
		SamBam_fclose(fp);
	}
	return 0;
}

int SamBam_writer_create(SamBam_Writer * writer, char * BAM_fname)
{
	memset(writer, 0, sizeof(SamBam_Writer));

	if(BAM_fname)
	{
		writer -> bam_fp = fopen(BAM_fname, "wb");
		if(!writer -> bam_fp) return -1;
	}
	#ifdef MAKE_STANDALONE
	else
		writer -> bam_fp = stdout;
	#endif
	writer -> chunk_buffer = malloc(70000); 
	writer -> compressed_chunk_buffer = malloc(70000); 
	writer -> chromosome_name_table = HashTableCreate(1603);
	writer -> chromosome_id_table = HashTableCreate(1603);
	writer -> chromosome_len_table = HashTableCreate(1603);
	writer -> header_plain_text_buffer = malloc(100000000);
	writer -> header_plain_text_buffer_max = 100000000;
	writer -> header_plain_text_buffer_used = 0;

	//memset(writer -> header_plain_text_buffer , 0 , 100000000);
	HashTableSetHashFunction(writer -> chromosome_name_table , fc_chro_hash);
	HashTableSetKeyComparisonFunction(writer -> chromosome_name_table , fc_strcmp_chro);
	HashTableSetDeallocationFunctions(writer -> chromosome_name_table , free, NULL);

	return 0;
}

void SamBam_writer_chunk_header(SamBam_Writer * writer, int compressed_size)
{

	// the four magic characters
	fputc(31,  writer -> bam_fp);
	fputc(139,  writer -> bam_fp);
	fputc(8,  writer -> bam_fp);
	fputc(4,  writer -> bam_fp);

	time_t time_now = 0;
	fwrite(&time_now,4,1, writer -> bam_fp);

	int tmp_i;
	// Extra flags and OS
	fputc(0,  writer -> bam_fp);
	fputc(0xff,  writer -> bam_fp); 

	// Extra length
	tmp_i = 6;
	fwrite(&tmp_i,2,1, writer -> bam_fp);


	// SI1 and SI2 magic numbers, and SLEN
	fputc(66,  writer -> bam_fp);
	fputc(67,  writer -> bam_fp);
	tmp_i = 2;
	fwrite(&tmp_i,2,1, writer -> bam_fp);
	tmp_i = compressed_size + 19 + 6;
	fwrite(&tmp_i,2,1, writer -> bam_fp);
}

unsigned int SamBam_CRC32(char * dat, int len)
{
	unsigned int crc0 = crc32(0, NULL, 0);
	unsigned int ret = crc32(crc0, (unsigned char *)dat, len);
	return ret;
}

void SamBam_writer_add_chunk(SamBam_Writer * writer)
{
	int compressed_size ; 
	unsigned int CRC32;
	writer -> output_stream.avail_out = 70000;
	writer -> output_stream.avail_in = writer ->chunk_buffer_used;
	CRC32 = SamBam_CRC32(writer -> chunk_buffer , writer ->chunk_buffer_used);

	//FILE * dfp = fopen("my.xbin","ab");
	//fwrite( writer ->chunk_buffer,  writer ->chunk_buffer_used, 1, dfp);
	//fclose(dfp);

 	int Z_DEFAULT_MEM_LEVEL = 8;
	writer -> output_stream.zalloc = Z_NULL;
	writer -> output_stream.zfree = Z_NULL;
	writer -> output_stream.opaque = Z_NULL;

	deflateInit2(&writer -> output_stream, SAMBAM_COMPRESS_LEVEL, Z_DEFLATED,
		SAMBAM_GZIP_WINDOW_BITS, Z_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	
	writer -> output_stream.next_in = (unsigned char *)writer -> chunk_buffer;
	writer -> output_stream.next_out = (unsigned char *)writer -> compressed_chunk_buffer;

	deflate(&writer -> output_stream, Z_FINISH);
	deflateEnd(&writer -> output_stream);

	compressed_size = 70000 - writer -> output_stream.avail_out;
	//printf("ADDED BLOCK=%d; LEN=%d; S=%s\n", compressed_size, writer ->chunk_buffer_used,  writer ->chunk_buffer);
	SamBam_writer_chunk_header(writer, compressed_size);
	fwrite(writer -> compressed_chunk_buffer, 1, compressed_size, writer -> bam_fp);

	fwrite(&CRC32 , 4, 1, writer -> bam_fp);
	fwrite(&writer ->chunk_buffer_used , 4, 1, writer -> bam_fp);

	writer ->chunk_buffer_used = 0;


}

void SamBam_writer_write_header(SamBam_Writer * writer)
{
	int header_ptr=0, header_block_start = 0;
	while(header_ptr < writer->header_plain_text_buffer_used)
	{
		if(( header_ptr - header_block_start > 55000 || header_ptr >= writer->header_plain_text_buffer_used-1) && writer -> header_plain_text_buffer[header_ptr] == '\n')
		{
			writer -> chunk_buffer_used = 0;
			if(header_block_start == 0)	// the very first block
			{
				memcpy(writer -> chunk_buffer, "BAM\1",4);
				writer -> chunk_buffer_used  = 4;
				memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used, &writer -> header_plain_text_buffer_used, 4);
				writer -> chunk_buffer_used += 4;
		
			}

			memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , writer -> header_plain_text_buffer + header_block_start, header_ptr - header_block_start+1);
			writer -> chunk_buffer_used +=  header_ptr - header_block_start + 1;
			SamBam_writer_add_chunk(writer);
			header_block_start = header_ptr + 1;
		}
		header_ptr ++;
	}

	free(writer -> header_plain_text_buffer);
	writer -> header_plain_text_buffer = NULL;

	// reference sequences
	writer -> chunk_buffer_used = 0;
	memcpy(writer -> chunk_buffer, & writer -> chromosome_name_table -> numOfElements, 4);
	writer -> chunk_buffer_used = 4;

	for( header_ptr=0 ;  header_ptr < writer -> chromosome_name_table -> numOfElements ; header_ptr ++)
	{
		//printf("D=%d\n", writer -> chromosome_id_table -> numOfElements);
		char * chro_name = HashTableGet(writer -> chromosome_id_table, NULL + 1 + header_ptr);
		unsigned int chro_len = HashTableGet(writer -> chromosome_len_table, NULL + 1 + header_ptr) - NULL - 1;
		assert(chro_name);
		int chro_name_len = strlen(chro_name)+1;

		memcpy(writer -> chunk_buffer +  writer -> chunk_buffer_used , &chro_name_len, 4);
		writer -> chunk_buffer_used += 4;

		strcpy(writer -> chunk_buffer +  writer -> chunk_buffer_used , chro_name);
		writer -> chunk_buffer_used += chro_name_len;

		memcpy(writer -> chunk_buffer +  writer -> chunk_buffer_used , &chro_len, 4);
		writer -> chunk_buffer_used += 4;

		if(header_ptr ==  writer -> chromosome_name_table -> numOfElements - 1 || writer -> chunk_buffer_used > 55000)
		{
			SamBam_writer_add_chunk(writer);
			writer -> chunk_buffer_used = 0;
		}
	}

}

int SamBam_writer_close(SamBam_Writer * writer)
{
	if(writer -> writer_state == 0)	// no reads were added
	{
		if(writer -> header_plain_text_buffer)
			SamBam_writer_write_header(writer);
	}
	else if(writer -> chunk_buffer_used)
		SamBam_writer_add_chunk(writer);
	
	writer -> chunk_buffer_used = 0;
	SamBam_writer_add_chunk(writer);
//	fputc(0, writer -> bam_fp);

	writer -> output_stream.next_in= NULL;
	writer -> output_stream.avail_in= 0;
	writer -> output_stream.next_out= NULL;
	writer -> output_stream.avail_out= 0;

	free(writer -> chunk_buffer);
	free(writer -> compressed_chunk_buffer);
	HashTableDestroy(writer -> chromosome_name_table);
	HashTableDestroy(writer -> chromosome_id_table);
	HashTableDestroy(writer -> chromosome_len_table);
	#ifdef MAKE_STANDALONE
	if(stdout != writer -> bam_fp)
	#endif
	fclose(writer -> bam_fp);

	return 0;
}

int SamBam_writer_add_header(SamBam_Writer * writer, char * header_text)
{
	int new_text_len = strlen(header_text);

	if(writer -> header_plain_text_buffer_max <= writer -> header_plain_text_buffer_used + new_text_len + 1)
	{
		//return 0;
		writer -> header_plain_text_buffer_max *=2;
		writer -> header_plain_text_buffer = realloc(writer -> header_plain_text_buffer ,  writer -> header_plain_text_buffer_max);
		//printf("REAL: %d : %llX\n",writer -> header_plain_text_buffer_max, (long long ) writer -> header_plain_text_buffer);
	}

	strcpy(writer -> header_plain_text_buffer + writer -> header_plain_text_buffer_used, header_text);
	writer -> header_plain_text_buffer_used += new_text_len;
	strcpy(writer -> header_plain_text_buffer + writer -> header_plain_text_buffer_used, "\n");
	writer -> header_plain_text_buffer_used ++;

	//if(writer -> header_plain_text_buffer_used %97==0) printf("MV=%d\n",writer -> header_plain_text_buffer_used);

	return 0;
}

int SamBam_writer_add_chromosome(SamBam_Writer * writer, char * chro_name, unsigned int chro_length)
{
	unsigned int chro_id = writer -> chromosome_name_table -> numOfElements;
	char * line_buf = malloc(1000);

	//assert(strlen(chro_name) < 30);

	char * chro_name_space = malloc(strlen(chro_name)+1);
	strcpy(chro_name_space , chro_name);
	HashTablePut(writer -> chromosome_name_table, chro_name_space, NULL+1+chro_id);
	HashTablePut(writer -> chromosome_id_table, NULL+1+chro_id, chro_name_space);
	HashTablePut(writer -> chromosome_len_table, NULL+1+chro_id, NULL + 1 + chro_length);
	snprintf(line_buf,999, "@SQ\tSN:%s\tLN:%u", chro_name , chro_length);
	SamBam_writer_add_header(writer, line_buf);
	free(line_buf);

	return 0;
}

int SamBam_compress_cigar(char * cigar, int * cigar_int)
{
	int tmp_int=0;
	int cigar_cursor = 0, num_opt = 0;

	if(cigar[0]=='*') return 0;
	
	while(1)
	{
		char nch = cigar[cigar_cursor++];
		if(!nch)break;
		if(isdigit(nch))
		{
			tmp_int = tmp_int*10+(nch-'0');
		}
		else
		{
			int int_opt=0;
			for(; int_opt<8; int_opt++) if("MIDNSHP=X"[int_opt] == nch)break;
			cigar_int[num_opt ++] = (tmp_int << 4) | int_opt; 
			tmp_int = 0;
			if(num_opt>=12)break;
		}
	}
	return num_opt;
}

void SamBam_read2bin(char * read_txt, char * read_bin)
{
	int bin_cursor = 0, txt_cursor = 0;

	while(1)
	{
		char nch = read_txt[txt_cursor++];
		if(!nch)break;
		int fourbit;
		for(fourbit=0;fourbit<15;fourbit++) if("=ACMGRSVTWYHKDBN"[fourbit] == nch)break;

		if(bin_cursor %2 == 0)  read_bin[bin_cursor/2] =  fourbit<<4;
		else read_bin[bin_cursor/2] |=  fourbit;

		bin_cursor++;
	}
}

int SamBam_compress_additional(char * additional_columns, char * bin)
{
	int col_cursor = 0 , col_len = strlen(additional_columns);
	int bin_cursor = 0;

	while(col_cursor<col_len)
	{
		if(col_cursor==0 || additional_columns[col_cursor]=='\t')
		{
			if(additional_columns[col_cursor]=='\t') col_cursor++;

			bin[bin_cursor] = additional_columns[col_cursor];
			bin[bin_cursor+1] = additional_columns[col_cursor+1];

			char datatype = additional_columns[col_cursor+3];
			if(datatype=='i')
			{
				int dig_len =0;
				while(additional_columns[dig_len+col_cursor+5] != '\t' && additional_columns[dig_len+col_cursor+5]) dig_len++;
				int val = atoi(additional_columns+col_cursor+5);
				bin[bin_cursor+2]='i';
				memcpy(bin+bin_cursor+3, &val,4);
				bin_cursor += 3 + 4;
				col_cursor += 5 + dig_len;
			}
			else if(datatype=='Z')
			{
				bin[bin_cursor+2]='Z';
				bin_cursor +=3;
				int str_len = 0;
				col_cursor +=5;
				while(additional_columns[str_len+col_cursor] != '\t' && additional_columns[str_len+col_cursor])
				{
					bin[bin_cursor + str_len] = additional_columns[str_len+col_cursor];
					str_len++;
				}

				bin[bin_cursor + str_len] =0;

				bin_cursor += str_len + 1;
				col_cursor += str_len;
			}
			if(bin_cursor>250) break;
			continue;
		}
		
		col_cursor++;
	}
	return bin_cursor;
}

int SamBam_writer_add_read(SamBam_Writer * writer, char * read_name, unsigned int flags, char * chro_name, unsigned int chro_position, int mapping_quality, char * cigar, char * next_chro_name, unsigned int next_chro_position, int temp_len, int read_len, char * read_text, char * qual_text, char * additional_columns)
{
	if(writer -> writer_state == 0)	// no reads were added
	{
		if(writer -> header_plain_text_buffer)
			SamBam_writer_write_header(writer);
	}
	writer -> writer_state = 10;
	char additional_bin[300];
	int cigar_opts[12], xk1;
	int cigar_opt_len = SamBam_compress_cigar(cigar, cigar_opts);
	int read_name_len = 1+strlen(read_name) ;
	int additional_bin_len = SamBam_compress_additional(additional_columns, additional_bin);
	int record_length = 4 + 4 + 4 + 4 +  /* l_seq: */ 4 + 4 + 4 + 4 + /* read_name:*/ read_name_len + cigar_opt_len * 4 + (read_len + 1) /2 + read_len + additional_bin_len;

	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & record_length , 4);
	writer -> chunk_buffer_used += 4;

	int refID = HashTableGet(writer -> chromosome_name_table, chro_name) - NULL - 1; 
	int bin_mq_nl = (mapping_quality << 8) | read_name_len ;
	int fag_nc = (flags<<16) | cigar_opt_len;
	int nextRefID = -1;

	if(next_chro_name[0] != '*' && next_chro_name[0]!='=')
		nextRefID = HashTableGet(writer -> chromosome_name_table, next_chro_name) - NULL - 1;
	else if(next_chro_name[0] == '=')
		nextRefID = refID;

	
	chro_position--;
	next_chro_position--;

	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & refID , 4);
	writer -> chunk_buffer_used += 4;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & chro_position , 4);
	writer -> chunk_buffer_used += 4;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & bin_mq_nl , 4);
	writer -> chunk_buffer_used += 4;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & fag_nc , 4);
	writer -> chunk_buffer_used += 4;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & read_len , 4);
	writer -> chunk_buffer_used += 4;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & nextRefID , 4);
	writer -> chunk_buffer_used += 4;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & next_chro_position , 4);
	writer -> chunk_buffer_used += 4;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , & temp_len , 4);
	writer -> chunk_buffer_used += 4;
	strcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , read_name);
	writer -> chunk_buffer_used += read_name_len;
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used , cigar_opts, 4*cigar_opt_len);
	writer -> chunk_buffer_used += 4*cigar_opt_len;
	SamBam_read2bin(read_text  , writer -> chunk_buffer + writer -> chunk_buffer_used);
	writer -> chunk_buffer_used += (read_len + 1) /2; 
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used, qual_text, read_len);
	for(xk1=0; xk1<read_len; xk1++)
		writer -> chunk_buffer[writer -> chunk_buffer_used+xk1] -= 33;
	
	writer -> chunk_buffer_used += read_len; 
	memcpy(writer -> chunk_buffer + writer -> chunk_buffer_used, additional_bin, additional_bin_len);
	writer -> chunk_buffer_used += additional_bin_len;


	if(writer -> chunk_buffer_used>55000)
	{
		SamBam_writer_add_chunk(writer);
		writer -> chunk_buffer_used = 0;
	}	
	return 0;
}
// test function
#ifdef MAKE_TEST_SAMBAM
int main(int argc , char ** argv)
{
	test_bam_compress();
}


void test_bam_compress()
{
	SamBam_Writer writer;
	if(SamBam_writer_create(&writer , "my.bam")) printf("INIT ERROR\n");

	SamBam_writer_add_header(&writer, "@RG	ID:xxhxh");
	SamBam_writer_add_chromosome(&writer, "chr1", 123123);
	SamBam_writer_add_chromosome(&writer, "chr2", 223123);
	SamBam_writer_add_header(&writer, "@PG	ID:subread	VN:1.4.0b2");
	SamBam_writer_add_read(& writer, "Read1", 0, "chr1", 100000, 200, "50M", "*", 0, 0, 50, "ATCGAATCGAATCGAATCGAATCGAATCGAATCGAATCGAATCGAATCGA", "AAAAABBBBBAAAAABBBBBAAAAABBBBBAAAAABBBBBAAAAABBBBB", "XG:Z:OX	NM:i:2	RG:Z:MyGroup1");
	SamBam_writer_add_read(& writer, "Read2", 16, "chr2", 200000, 200, "50M", "*", 0, 0, 50, "ATCGAATCGAATCGAATCGAATCGAATCGAATCGAATCGAATCGAATCGA", "AAAAABBBBBAAAAABBBBBAAAAABBBBBAAAAABBBBBAAAAABBBBB", "NM:i:1	XX:i:8172736	RG:Z:nxnmn	XY:i:33999	XZ:Z:Zuzuzu");
	SamBam_writer_close(&writer);
}
#endif
