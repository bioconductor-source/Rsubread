/***************************************************************

   The Subread and Rsubread software packages are free
   software packages:
 
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
#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "gene-value-index.h"
#include "input-files.h"


int gvindex_init(gene_value_index_t * index, unsigned int start_point, unsigned int base_number)
{
	index->start_point = start_point;
	index->length = base_number;
	index->values = malloc(base_number / 4 + 1);
	if(!index->values)
	{
		SUBREADputs(MESSAGE_OUT_OF_MEMORY);
		return 1;
	}
	index -> start_base_offset = index -> start_point - index -> start_point%4;
	return 0;
}

#define gvindex_baseno2offset_m(base_number, index, offset_byte, offset_bit)	{offset_byte =  (base_number - index -> start_base_offset) >>2; offset_bit = base_number % 4 * 2;}


void gvindex_baseno2offset(unsigned int base_number, gene_value_index_t * index, unsigned int * offset_byte, unsigned int * offset_bit)
{
	// the base number corrsponding to the 0-th bit in the whole value array;

	unsigned int offset = (base_number - index -> start_base_offset);

	* offset_byte = offset >>2 ;
	* offset_bit = base_number % 4 * 2;
}

// return 'A', 'G', 'T' and 'C'
int gvindex_get(gene_value_index_t * index, gehash_data_t offset)
{
	unsigned int offset_byte, offset_bit;
	gvindex_baseno2offset_m(offset, index , offset_byte, offset_bit);

	if(offset_byte >= index-> values_bytes)return 'N';

	unsigned int one_base_value = (index->values [offset_byte]) >> (offset_bit);

	return int2base(one_base_value & 3);
}

int gvindex_match(gene_value_index_t * index, gehash_data_t offset, gehash_key_t base_values)
{
	unsigned int offset_byte, offset_bit;

	gvindex_baseno2offset_m(offset, index , offset_byte, offset_bit);
	int i, ret = 0;

	for (i=0; i<16; i++)
	{
		unsigned char mask = 0x3 << (offset_bit);
		unsigned char one_base_value = (index->values [offset_byte] & mask) >> (8-offset_bit);
		if ( ((base_values >> (30 - i*2)) & 0x3) == one_base_value)
			ret |= 1 << i;

		offset_bit +=2;
		if(offset_bit >=8)
		{
			offset_bit = 0;
			offset_byte ++;
		}
	}

	return ret;

}

void gvindex_set (gene_value_index_t * index, gehash_data_t offset, gehash_key_t base_values)
{
	unsigned int offset_byte, offset_bit;
	gvindex_baseno2offset(offset, index , &offset_byte, &offset_bit);
	int i;

	for (i=0; i<16; i++)
	{
		// 11110011
		//     ^^ base
		unsigned char mask = 0xff << (offset_bit+2) | 0xff >> (8-offset_bit);
		index->values [offset_byte] &= mask;
		index->values [offset_byte] |= ((base_values >> (30 - i*2))&0x03) << (offset_bit);

		offset_bit +=2;
		if(offset_bit >=8)
		{
			offset_bit = 0;
			offset_byte ++;
		}
	}

	index -> length = offset + 16 - index -> start_point ;
}

void gvindex_dump(gene_value_index_t * index, const char filename [])
{
	FILE * fp = fopen(filename, "wb");

	fwrite(&index->start_point,4,1, fp);
	fwrite(&index->length, 4, 1, fp);

	unsigned int useful_bytes, useful_bits;
	gvindex_baseno2offset (index -> length+ index -> start_point, index,&useful_bytes,&useful_bits);

	fwrite(index->values, 1, useful_bytes, fp);

	fclose(fp);
}


int gvindex_load(gene_value_index_t * index, const char filename [])
{
	FILE * fp = fopen(filename, "rb");
	int read_length;
	read_length = fread(&index->start_point,4,1, fp);
	assert(read_length>0);
	read_length = fread(&index->length,4,1, fp);
	assert(read_length>0);

	//SUBREADprintf ("\nBINDEX %s : %u ~ +%u\n",filename, index->start_point, index->length );

	unsigned int useful_bytes, useful_bits;
	index -> start_base_offset = index -> start_point - index -> start_point%4;
	gvindex_baseno2offset (index -> length+ index -> start_point, index ,&useful_bytes,&useful_bits);
	index -> values = malloc(useful_bytes);
	index -> values_bytes = useful_bytes;
	if(!index->values)
	{
		SUBREADputs(MESSAGE_OUT_OF_MEMORY);
		return 1;
	}
	

	read_length =fread(index->values, 1, useful_bytes, fp);
	assert(read_length>0);

	fclose(fp);
	return 0;

}


int match_chro_wronglen(char * read, gene_value_index_t * index, unsigned int pos, int test_len, int space_type, int * left_match_bases, int * right_match_bases)
{
	int ret = 0;
	int i;
	char last_char='A';
	int left_correct_end = 0;
	if(left_match_bases) *left_match_bases=0;
	if(right_match_bases) *right_match_bases=0;

	if (space_type == GENE_SPACE_COLOR)
		last_char = (pos <= index -> start_point)?'A': gvindex_get(index,pos-1);

	for (i=0;i<test_len;i++)
	{
		char tt = gvindex_get (index, pos +i);
		int newv;
		if(space_type == GENE_SPACE_COLOR)
		{

			newv = read[i] == '0'+chars2color(last_char, tt); 
			last_char = tt;
		}
		else
			newv =read[i] == tt; 

		//if(left_wrong_bases)
		//	SUBREADprintf("I=%d, *LWB=%d, LWE=%d\n", i, *left_wrong_bases, left_wrong_end);

		if(left_match_bases && (newv) && (!left_correct_end ))
			(*left_match_bases)++;
		else if (!newv)left_correct_end=1;

		if(right_match_bases && (newv))
			(*right_match_bases) ++;
		else if (right_match_bases)
			(*right_match_bases) =0;

		ret += newv;
	}

	return ret;
}

#define INDEL_TEST_WINDOW 3
#define MIN_INDEL_SEARCH_MATCH_SCORE 8500

int match_indel_chro_to_front(char * read, gene_value_index_t * index, unsigned int pos, int test_len, int * indels, int * indel_point, int max_indel_number, int max_test_length)
{
	int offset = 0;
	int i;
	int ret = 0;

	for(i=0; i < test_len+min(0,offset); i++)
	{
		unsigned int npos = pos + i + max(0, offset);
		char tt;
		tt = gvindex_get (index, npos);

		//if(i-min(0,offset) >= max_test_length)break;

		if(read[i-min(0,offset)]==tt)ret++;
		else if(i + offset < test_len - INDEL_TEST_WINDOW - 3 && i >0)
		{
			// if there is a base unmatched, it is potentially an indel from here.
			int bast_match_score_remailing=-1;
			int window_match = match_chro(read+i-min(0,offset), index, pos+i+ max(0, offset), INDEL_TEST_WINDOW ,0,GENE_SPACE_BASE);

			if(window_match < INDEL_TEST_WINDOW -1)
			{
				// if the window is badly matched, it is very likely to be an indel from this base.
				int indel_test_i;
				for(indel_test_i =0; indel_test_i < 7; indel_test_i++)
				{
					int indel_test = (indel_test_i+1)/2*(indel_test_i%2?1:-1);
					if(abs(indel_test)>max_indel_number) continue;

					if(indel_test > 0)	//insertion 
					{
						int matched_tail = match_chro(read+i, index, pos+i+indel_test, test_len - i,0,GENE_SPACE_BASE);
						int matched_score = matched_tail * 10000 / ( test_len - i);
						if(matched_score >  bast_match_score_remailing &&  matched_score > MIN_INDEL_SEARCH_MATCH_SCORE)
						{
							offset = indel_test;
							bast_match_score_remailing = matched_score;
						}
					}else	// deletion
					{
						int matched_tail = match_chro(read+i - indel_test, index, pos+i, test_len - i + indel_test ,0,GENE_SPACE_BASE);
						int matched_score = matched_tail * 10000 / (test_len - i + indel_test);
						if(matched_score >  bast_match_score_remailing &&  matched_score > MIN_INDEL_SEARCH_MATCH_SCORE)
						{
							offset = indel_test;
							bast_match_score_remailing = matched_score;
						}
					}
				}
			}
			if(bast_match_score_remailing>0)
			{
				if(offset > 0)//deletion
				{
					npos = pos + i + offset;
					tt = gvindex_get (index, npos);
					ret += read[i] == tt;
				}
				else
				{
					ret += read[i - offset] == tt;
				}
				*indel_point  = i;
				
			}
		}
	}
	*indels = offset;
	return ret;

}


// "pos" here is the expected position of the head of the read , but it is not the final position. If indels are found in the read, the head position must be offset.
// Only certain is pos+test_len is the EXACT position of the TAIL of the read.
int match_indel_chro_to_back(char * read, gene_value_index_t * index, unsigned int pos, int test_len, int * indels, int * indel_point, int max_indel_number, int min_test_offset)
{
	//return  match_chro(read, index, pos, test_len, 0, 1);
	//SUBREADprintf("TEST_INDEL_CHRO %s VS %u LEN=%d\n", read, pos, test_len);
	int offset = 0;
	int i;
	int ret = 0;


	if(pos > 0xffff0000 || pos + test_len>= index-> length + index->start_point) 
	{
		*indels = 0;
		return 0;
	}

	for(i=test_len-1 ; i >=0/*max(0,offset)*/; i--)
	{
		unsigned int npos = pos + i - max(0, offset);
		char tt;

		tt = gvindex_get (index, npos);

		#ifdef TEST_TARGET
		if(memcmp(read, TEST_TARGET, 15)==0)
			SUBREADprintf("%c=?=%c OFF=%d\n",read[i+min(0,offset)], tt, offset);
		#endif

		//if(i+min(0,offset) < min_test_offset) break;
		if(read[i+min(0,offset)]==tt)ret++;
		else if(i + offset >INDEL_TEST_WINDOW +3 && i < test_len-1)
		{
			// if there is a base unmatched, it is potentially an indel from here.
			int bast_match_score_remailing=-1;
			int window_match = match_chro(read+i-min(0,offset)-INDEL_TEST_WINDOW, index, pos+i+ max(0, offset)-INDEL_TEST_WINDOW, INDEL_TEST_WINDOW ,0,GENE_SPACE_BASE);

			if(window_match < INDEL_TEST_WINDOW-1)
			{
				// if the window is badly matched, it is very likely to be an indel from this base.
				int indel_test_i;
				for(indel_test_i =0; indel_test_i < 7; indel_test_i++)
				{
					int indel_test = (indel_test_i+1)/2*(indel_test_i%2?1:-1);
					if(abs(indel_test)>max_indel_number) continue;
					if(indel_test > 0)	//deletion 
					{
						int matched_tail = match_chro(read , index, pos-indel_test, i ,0,GENE_SPACE_BASE);
						int matched_score = matched_tail * 10000 / ( i );

						#ifdef TEST_TARGET
						if(memcmp(read, TEST_TARGET, 15)==0)
							SUBREADprintf("INDEL_DEL_TEST i=%d: Indel=%d, Score=%f, HEADPOS=%u\n",i, indel_test,matched_score , pos-indel_test );
						#endif

						if(matched_score >  bast_match_score_remailing &&  matched_score > MIN_INDEL_SEARCH_MATCH_SCORE)
						{
							offset = indel_test;
							bast_match_score_remailing = matched_score;
						}
					}else	//insertion 
					{
						int matched_tail = match_chro(read, index, pos - indel_test, i ,0,GENE_SPACE_BASE);
						int matched_score = matched_tail * 10000 / (i + indel_test);
						#ifdef TEST_TARGET
						if(memcmp(read, TEST_TARGET, 15)==0)
							SUBREADprintf("INDEL_INS_TEST i=%d: Indel=%d, Score=%f, HEADPOS=%u\n",i, indel_test,matched_score  , pos + indel_test);
						#endif
						if(matched_score >  bast_match_score_remailing &&  matched_score > MIN_INDEL_SEARCH_MATCH_SCORE)
						{
							offset = indel_test;
							bast_match_score_remailing = matched_score;
						}
					}
				}
			}
			if(bast_match_score_remailing>0)
			{
				#ifdef TEST_TARGET
				if(memcmp(read, TEST_TARGET, 15)==0)
					SUBREADprintf("PIECE_LEN=%d ; INDEL AT %d ; INDEL=%d\n", test_len , i + min(0, offset) , offset);
				#endif
				if(offset > 0)//insertion
				{
					ret += read[i-offset] == tt;
				}
				else	//deletion
				{
					npos = (pos + i - offset);
					tt = gvindex_get (index, pos + i - offset);
					ret += read[i] == tt;
				}
				*indel_point  = i + min(0, offset);
			}
		}
	}
	//if(memcmp(read, "AACCCCTTGCAGAAAA", 15)==0)
	//	SUBREADprintf("\nINDEL_SPOT %d\n", offset);
	*indels = offset;
	return ret;
}



float match_chro_support(char * read, gene_value_index_t * index, unsigned int pos, int test_len, int is_negative_strand, int space_type, char * qual_txt, int qual_format)
{
	int i;
	int all_qual = 0;
	int supported_qual = 0; 
				

	if (is_negative_strand)
	{

		for (i=test_len -1;i>=0;i--)
		{
			char tt = gvindex_get(index, pos+test_len-1-i);
			int is_correct = 0;
			switch(tt)
			{
				case 'A': is_correct = read[i] == 'T'; break;
				case 'T': is_correct = read[i] == 'A'; break;
				case 'G': is_correct = read[i] == 'C'; break;
				case 'C': is_correct = read[i] == 'G'; break;
			}

			int base_p = 0.;

			if(qual_txt[0])
			{
				if(FASTQ_PHRED64 == qual_format)
				{
					base_p = get_base_error_prob64i(qual_txt[i]);
				}
				else
				{
					base_p = get_base_error_prob33i(qual_txt[i]);
				}
			}

			if(base_p > 300000) continue;
			base_p=0;
			all_qual += (1000000-base_p);

			if(is_correct)
				supported_qual += (1000000-base_p);
		}
	}
	else
	{
		if(qual_txt[0])
		{
			if(FASTQ_PHRED33 == qual_format)
			{
				for (i=0;i<test_len;i++)
				{
					char tt = gvindex_get (index, pos +i);
					int is_correct =read[i] == tt; 
					int base_p = get_base_error_prob33i(qual_txt[i]);

					if(base_p > 300000) continue;
					base_p=0;

					all_qual += (1000000-base_p);
	
					if(is_correct)
						supported_qual += (1000000-base_p);
				}
			}
			else
			{
				for (i=0;i<test_len;i++)
				{
					char tt = gvindex_get (index, pos +i);
					int is_correct =read[i] == tt; 
					int base_p = get_base_error_prob64i(qual_txt[i]);

					if(base_p > 300000) continue;
					base_p=0;

					all_qual += (1000000-base_p);
	
					if(is_correct)
						supported_qual += (1000000-base_p);
				}
			}
		}
		else
			for (i=0;i<test_len;i++)
			{
				char tt = gvindex_get (index, pos +i);
				int is_correct =read[i] == tt; 
				
				all_qual += (1000000);

				if(is_correct)
					supported_qual += (1000000);
			}	
	}

	//SUBREADprintf("%d\n", test_len);
	//if(all_qual < 3100000) return 0;
	if(all_qual < 3100000) return 0;
	return supported_qual*1. / all_qual * test_len;
}




int match_chro(char * read, gene_value_index_t * index, unsigned int pos, int test_len, int is_negative_strand, int space_type)
{

	
	int ret = 0;
	int i;
	char last_char='A';
	
	if ((unsigned int)(pos + test_len) >= index -> length + index -> start_point) return 0;
	if (pos > 0xffff0000) return 0;

	if (is_negative_strand)
	{

		if (space_type == GENE_SPACE_COLOR)
		{
			pos++;
			last_char = (pos+test_len>= index -> length + index -> start_point)?'A': gvindex_get(index,pos+test_len);
			for (i=test_len -1;i>=0;i--)
			{
				char tt = gvindex_get (index, pos+test_len-1-i);
				ret += read[i] == '0'+chars2color(tt, last_char); 
				last_char = tt;
			}
		}
		else
		{

			for (i=test_len -1;i>=0;i--)
			{
				char tt = gvindex_get (index, pos+test_len-1-i);
				switch(tt)
				{
					case 'A': ret += read[i] == 'T'; break;
					case 'T': ret += read[i] == 'A'; break;
					case 'G': ret += read[i] == 'C'; break;
					case 'C': ret += read[i] == 'G'; break;
				}
			}
	


		}
	}	
	else
	{
		if(space_type == GENE_SPACE_BASE)
		{
			unsigned int offset_byte, offset_bit;

			gvindex_baseno2offset_m(pos, index , offset_byte, offset_bit);

			if(offset_byte >= index-> values_bytes)return 0;
			char int_value = index->values [offset_byte];

			for (i=0;i<test_len;i++)
			{
				char tt = (int_value >> offset_bit) & 3;
				char tv = read[i];
				switch(tv){
					case 'A':
						ret += tt==0;
						break;
					case 'G':
						ret += tt==1;
						break;
					case 'C':
						ret += tt==2;
						break;
					default:
						ret += tt==3;
						break;

				}
				offset_bit+=2;
				if(offset_bit==8)
				{
					offset_byte++;
					if(offset_byte == index-> values_bytes)return 0;
					int_value = index->values [offset_byte];
					offset_bit = 0;
				}
			}
		}
		else
		{
			last_char = (pos <= index -> start_point)?'A': gvindex_get(index,pos-1);
			for (i=0;i<test_len;i++)
			{
				char tt = gvindex_get (index, pos +i);
				ret += read[i] == '0'+chars2color(last_char, tt);
				last_char = tt;
			}
		}
		
	}
	return ret;
}


int match_chro_slow(char * read, gene_value_index_t * index, unsigned int pos, int test_len, int is_negative_strand, int space_type)
{
	if(is_negative_strand || space_type == GENE_SPACE_COLOR)
		return match_chro_slow(read, index, pos, test_len, is_negative_strand, space_type);


	unsigned int i;
	unsigned int test_end = test_len+pos- index->start_base_offset;
	int ret = 0, offset, bits;
	for(i=(pos - index->start_base_offset); i< test_end ; i++)
	{
		offset = i/4;
		bits = i%4*2;
		ret += ((index -> values[offset] >> bits) & 0x3 )== base2int(*read);
		read++;
	}

	return ret;
}

unsigned int match_chro_range(char * read, gene_value_index_t * index, unsigned int pos, int read_len, int search_length, int search_to_back)
{
	short key[4];
	int i, j;
	for(i = 0; i < 4; i++)
	{
		key[i]=0;
		for(j = i+7; j >= i; j--)
		{
			key[i] = key[i] << 2 | base2int(read[j]);
		}
	}


	unsigned int offset_byte, offset_bit, search_dist;

	gvindex_baseno2offset(pos, index , &offset_byte, &offset_bit);

	//SUBREADprintf("POS=%u, OFFBYTE=%d\n" , pos, offset_byte);

	
	search_dist = search_length /4;
	if(search_to_back == SEARCH_BACK)
	{
		if(search_dist > offset_byte - 500)search_dist = offset_byte- 500;
	}
	else
	{
		if(search_dist + offset_byte >=  index -> values_bytes - 500) search_dist = index -> values_bytes - offset_byte -501;
	}


	for (i=2; i<search_dist; i++)
	{
		unsigned long test_offset = offset_byte;
		test_offset += (search_to_back == SEARCH_BACK)?-i:i;
		short tv = *(short *)(index->values +test_offset);

		for(j=0; j<4; j++)
		{
			if(tv == key[j])
			{
				//test the whole read
				unsigned int hit_pos = test_offset*4;
				hit_pos += index -> start_base_offset - j;// + index -> start_point; 
				int retv = match_chro_maxerror(read, index, hit_pos, read_len, 0, 0, 0);
				if(retv >0)
				{
					//SUBREADprintf("POS=%u, TOFF=%u, STARTBASE=%d, j=%d , RETV=%d\n" , test_offset*4 + index -> start_base_offset - j , test_offset , index -> start_base_offset , j , retv);
					return hit_pos; 
				}
			}
		}
	}
	return 0xffffffff;
}

int match_chro_maxerror(char * read, gene_value_index_t * index, unsigned int pos, int test_len, int is_negative_strand, int space_type, int max_error)
{
	int ret = 0;
	int i;
	char last_char='A';
	

	if (is_negative_strand)
	{

		if (space_type == GENE_SPACE_COLOR)
		{
			pos++;
			last_char = (pos+test_len>= index -> length + index -> start_point)?'A': gvindex_get(index,pos+test_len);
		}
		for (i=test_len -1;i>=0;i--)
		{
			char tt = gvindex_get (index, pos+test_len-1-i);
			if(space_type == GENE_SPACE_COLOR)
			{
				ret += read[i] != '0'+chars2color(tt, last_char); 
				last_char = tt;
			}
			else
				switch(tt)
				{
					case 'A': ret += read[i] != 'T'; break;
					case 'T': ret += read[i] != 'A'; break;
					case 'G': ret += read[i] != 'C'; break;
					case 'C': ret += read[i] != 'G'; break;
				}
			if(ret>max_error)return 0;
		}
	}
	else
	{
		if (space_type == GENE_SPACE_COLOR)
			last_char = (pos <= index -> start_point)?'A': gvindex_get(index,pos-1);
		for (i=0;i<test_len;i++)
		{
			char tt = gvindex_get (index, pos +i);
			if(space_type == GENE_SPACE_COLOR)
			{
				ret += read[i] != '0'+chars2color(last_char, tt);
				last_char = tt;
			}
			else
				ret +=read[i] != tt; 
			//SUBREADprintf("RET=%d\n",ret);
			if(ret>max_error)return 0;
		}
	}
	return test_len - ret;
}




int gvindex_match_base(gene_value_index_t * index, gehash_data_t offset, const char base_int_value)
{
	unsigned int offset_byte, offset_bit;

	gvindex_baseno2offset(offset, index, &offset_byte, &offset_bit);

	unsigned char mask = 0x3 << (offset_bit);

	if(offset_byte >= index->values_bytes)
		return 0;
//		SUBREADprintf("\nERROR: %u > %u\n", offset_byte, index->values_bytes);

	char reference_base = ((index->values [offset_byte] & mask) >> offset_bit);

	return  (reference_base == base_int_value)?1:0 ;
}

void gvindex_destory(gene_value_index_t * index)
{
	free(index -> values);
}


void gvindex_get_string(char *buf, gene_value_index_t * index, unsigned int pos, int len, int is_negative_strand)
{
	int i;
	if (is_negative_strand)
		for (i=len-1;i>=0;i--)
		{
			buf[i] = gvindex_get (index, pos + len - 1 - i);
			switch(buf[i])
			{
				case 'A': buf[i] = 'T'; break;
				case 'T': buf[i] = 'A'; break;
				case 'G': buf[i] = 'C'; break;
				case 'C': buf[i] = 'G'; break;
			}
		}
	else
		for (i=0;i<len;i++)
			buf[i] = gvindex_get (index, pos +i);
}


