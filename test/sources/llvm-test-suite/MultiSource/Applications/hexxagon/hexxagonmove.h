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

/*
 * Hexxagon board game.
 * Copyright (C) 2001 Erik Jonsson.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 * Email erik@nesqi.homeip.net
 * 
 */

#ifndef _HEXXAGONMOVE_H
#define _HEXXAGONMOVE_H

#include "hexxagonboard.h"

#define NO_MOVES_BEGIN 16

class HexxagonBoard;

class HexxagonMove
{
 public:
	HexxagonMove() {};
	HexxagonMove(int t) {from = t; to = t;};
	HexxagonMove(int f, int t) {from = f; to = t;};

	char from, to;
	int score;
};

class HexxagonMoveList
{
 public:
	HexxagonMoveList() { moves = new HexxagonMove[NO_MOVES_BEGIN]; nr_moves = 0; };
	~HexxagonMoveList() { delete moves; };
 
	void scoreAllMoves(HexxagonBoard board, int depth, void (*callback)(), int maxtime);
	void sortList();
	void sortListQuick(); /* Not used... (slower) */

	int getNrMoves();

	void addMove(HexxagonMove &move);
	HexxagonMove *getMove(int i);
	
	HexxagonMove *getBestMove();

 private:

	int nr_moves;
	HexxagonMove *moves;
};

#endif // _HEXXAGONMOVE_H
