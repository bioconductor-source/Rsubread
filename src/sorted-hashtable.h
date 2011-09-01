#ifndef _SORTED_HASHTABLE_H_
#define _SORTED_HASHTABLE_H_
#include <stdlib.h>
#include <stdio.h>

#include "subread.h"
#include "gene-algorithms.h"
#include "gene-value-index.h"

#define GEHASH_DEFAULT_SIZE	2000000000
#define GEHASH_BUCKET_LENGTH	2291


#define gehash_fast_t gehash_t
#define gehash_destory_fast gehash_destory


// This function creates a new hash table. The invoter may provide the expected size of the table or -1 for a default size (2 billions)
// This function returns 0 if success or an errno
int gehash_create(gehash_t * the_table, size_t expected_size, char is_small_table);

// This function puts a data item into the table. If there is duplication, it insert another copy into the table but do not overlap on the old one.
void gehash_insert(gehash_t * the_table, gehash_key_t key, gehash_data_t data);

// This function queries the table and put the matched data item into data_result.
// This function returns 0 if not found, or the number of matched items.
// The invoter is in charge of allocating memory for results.
size_t gehash_get(gehash_t * the_table, gehash_key_t key, gehash_data_t * data_result, size_t max_result_space);

// Test existance, disregarding numbers.
// Return 1 if exist, 0 if not.
int gehash_exist(gehash_t * the_table, gehash_key_t key);

size_t gehash_go_q(gehash_t * the_table, gehash_key_t key, int offset, int read_len, int is_reversed, gene_vote_t * vote,int is_add, gene_vote_number_t weight, gene_quality_score_t quality, int max_match_number, int indel_tolerance, int subread_number);

// This function performs the same functionality, but runs only on AMD-64 cpus, and the length of each key must be 4 bytes.
size_t gehash_get_hpc(gehash_t * the_table, gehash_key_t key, gehash_data_t * data_result, size_t max_result_space);

// This function removes all items under the key. It returns the number of items that has been removed in this call.
size_t gehash_remove(gehash_t * the_table, gehash_key_t key);

// Free all memory that is allocated for the table. Only the table structure itself is not freed.
void gehash_destory(gehash_t * the_table);

// This function conpletely dumps a table into a disk file.
// It returns 0 if success, otherwise -1.
int gehash_dump(gehash_t * the_table, const char fname []);

// This function loads a dumpped hash table.
// The invoker does not need to initialise the table; it will be initialised in the function.
// It returns 0 if success, otherwise -1.
int gehash_load(gehash_t * the_table, const char fname []);

void gehash_prealloc(gehash_t * the_table);

#endif
