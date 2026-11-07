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

#ifndef __FUNCTS_H
#define __FUNCTS_H

#include "types.h"

void speedup_test(FILE *fp);
uint32 imix_test(FILE *fp);
void specul_time_o(epoch_t *epoch, int num_epochs, FILE *graphfile, int show_speedup, int thread_pen, int commit_pen);
void specul_time_r(epoch_t *epoch, int num_epochs, int cpulimit, FILE *graphfile, int show_speedup, int thread_pen, int commit_pen);
void find_hard_raws();

#endif /* __FUNCTS_H */
