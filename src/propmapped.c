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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int propmapped(int argc,char *argv[]){

/*
  if(argc == 1){
    printf("Usage: propmapped sam_file\n");
    exit(0);
  }
*/

  FILE *fp, *fp_out;
  fp = fopen(argv[1],"r");
  fp_out = fopen(argv[2],"a+");

  char * line = NULL;
  size_t len = 0;
  ssize_t z;
  int unmapped = 0;
  int totalreads = 0;

  int MAX_LINE_LENGTH = 100000;
  
  line = (char*)calloc(MAX_LINE_LENGTH, 1);
  while (fgets(line, MAX_LINE_LENGTH, fp)) {
    if(line[0] == '@')
      continue;
    else
      totalreads++;

    if(*(index((index(line,'\t')+1),'\t')+1) == '*')
      unmapped++;
  }

  if (line)
    free(line);
 
  fclose(fp);

  // printf("Total number of reads is: %d\n", totalreads);
  // printf("Proportion of mapped reads is %f\n\n",1-(float)unmapped/totalreads);

  fprintf(fp_out,"%s,%d,%d,%f\n",argv[1],totalreads,totalreads-unmapped,1-(float)unmapped/totalreads);

  fclose(fp_out);
}
