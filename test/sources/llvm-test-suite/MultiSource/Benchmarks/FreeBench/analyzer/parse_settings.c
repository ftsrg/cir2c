/*
 * Copyright 2025 Budapest University of Technology and Economics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parse_settings.h"

#define NUM_TOKENS 17
char *pattern[NUM_TOKENS]={"LOAD_PENALTY","STORE_PENALTY",
			   "DATA","NAME",
			   "SPEED","EARLY_SPEED",
			   "NO_FORWARDING","INST_MIX",
			   "QUIET","EPOCH_LENGTH",
			   "KERNEL","CPULIMIT",
			   "CONFIGS","SHOW_SPEEDUP",
			   "THREAD_PEN","COMMIT_PEN",
			   "EPOCH_LENGTH_AS_NUM_EPOCHS"};
int settings[NUM_TOKENS];

int parse_settings(char *filename)
{
  FILE *fp;
  char *line;
  int i=0;

  line=(char *)malloc(100*sizeof(char));  
  for (i=0;i<NUM_TOKENS;i++)  
    settings[i]=-1;

  if ((fp=fopen(filename,"r"))==NULL) {
    fprintf(stderr,"Could not find file %s\n",filename);
    exit(1);
  }
  
  while(!feof(fp)) {
    i=0;
    fgets(line,100,fp);
    if (line[0]=='#') {
      continue;
    }
    strtok(line," ");
    while (i<NUM_TOKENS) {
      if (!strcmp(pattern[i],line)) {
	settings[i]=atoi(&line[strlen(line)+1]);
      }
      i++;
    }
  }

  free(line);

  return 0;
}
