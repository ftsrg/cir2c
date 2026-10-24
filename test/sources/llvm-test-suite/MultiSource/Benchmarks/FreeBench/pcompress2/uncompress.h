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

/********************************************************/
/* pCompress/pUnCompress                                */
/* A three stage file compressor, using Burrows Wheeler */
/* blocksorting, Run Length Encoding and Arithmetic     */
/* coding to achieve good compression.                  */
/* The fact that the compression is not that good in    */
/* reality is probably my fault.                        */
/* It makes a neat benchmark anyways.                   */
/* ---------------------------------------------------- */
/* This is a part of FreeBench v1 and is only intended  */
/* to be used as a benchmark. The use of this software  */
/* for anyting else (such as compression) is not        */
/* recomended, and certainly not supported. Use gzip or */
/* bzip instead, they are both faster and better.       */
/* Peter Rundberg, April 2001                           */
/********************************************************/

void uncompress(int argc, char *argv[]);
