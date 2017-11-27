// The MIT License(MIT)
//
// Copyright 2016-2017 Huldra
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// fjortris.cpp : Defines the entry point for the application.
#include "fjortris/fjortris.h"

#include <random>
#include "engine/easy.h"

using namespace arctic;  // NOLINT
using namespace arctic::easy;  // NOLINT

Sprite g_blocks[3];
std::independent_bits_engine<std::mt19937_64, 8, Ui64> g_rnd;
Si32 g_field[16][8] = {
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{2,0,0,0,0,0,0,2},
	{2,0,0,0,0,0,2,2},
	{2,0,0,0,0,2,2,2},
	{2,0,0,0,2,2,2,2},
	{2,0,0,0,2,2,2,2},
	{2,0,0,0,2,2,2,2},
};
Si32 g_tetraminoes[5][5 * 7] = {
	{0,0,1,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0},
	{0,0,1,0,0,  0,0,1,0,0,  0,0,1,0,0,  0,0,0,0,0,  0,0,1,1,0,  0,1,1,0,0,  0,0,1,1,0},
	{0,0,1,0,0,  0,0,1,0,0,  0,0,1,0,0,  0,1,1,1,0,  0,1,1,0,0,  0,0,1,1,0,  0,0,1,1,0},
	{0,0,1,0,0,  0,0,1,1,0,  0,1,1,0,0,  0,0,1,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0},
	{0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0},
};
Si32 g_current[4][5][5];
Si32 g_current_x;
Si32 g_current_y;
Si32 g_current_orientation;
double g_prev_time;
Si32 g_dx = 0;
Si32 g_dy = 0;
Si32 g_rotate = 0;
Si32 g_drop = 0;
void StartNewTetramino() {
	Si32 idx = g_rnd() % 7;
	for (Si32 y = 0; y < 5; ++y) {
		for (Si32 x = 0; x < 5; ++x) {
			g_current[0][y][x] = g_tetraminoes[y][x + idx * 5];
			g_current[1][x][4 - y] = g_tetraminoes[y][x + idx * 5];
			g_current[2][4 - y][4 - x] = g_tetraminoes[y][x + idx * 5];
			g_current[3][4 - x][y] = g_tetraminoes[y][x + idx * 5];
		}
	}
	g_current_x = 3;
	g_current_y = 0;
	g_current_orientation = g_rnd() % 4;
}
void ClearField() {
	for (Si32 y = 0; y < 16; ++y) {
		for (Si32 x = 0; x < 8; ++x) {
		    g_field[y][x] = 0;
		}
	}
}
void Init() {
	g_blocks[1].Load("data/block_1.tga");
	g_blocks[2].Load("data/block_2.tga");
	ResizeScreen(800, 500);
	StartNewTetramino();
	g_prev_time = Time();
}
bool IsPositionOk(Si32 test_x, Si32 test_y, Si32 test_orientation) {
	for (Si32 y = 0; y < 5; ++y) {
		for (Si32 x = 0; x < 5; ++x) {
			if (g_current[test_orientation][y][x]) {
				if (x + test_x < 0 || x + test_x > 7 || y + test_y > 15
					    || g_field[y + test_y][x + test_x]) {
					return false;
				}
			}
		}
	}
	return true;
}
void LockTetramino() {
	for (Si32 y = 0; y < 5; ++y) {
		for (Si32 x = 0; x < 5; ++x) {
			if (g_current[g_current_orientation][y][x]) {
				g_field[y + g_current_y][x + g_current_x] = 2;
			}
		}
	}
	bool do_continue = true;
	while (do_continue) {
		do_continue = false;
		for (Si32 y = 15; y >= 0; --y) {
			bool is_full_line = true;
			for (Si32 x = 0; x < 8; ++x) {
				if (!g_field[y][x]) {
					is_full_line = false;
					break;
				}
			}
			if (is_full_line) {
				do_continue = true;
				for (Si32 y2 = y; y2 > 0; --y2) {
					for (Si32 x = 0; x < 8; ++x) {
						g_field[y2][x] = g_field[y2 - 1][x];
					}
				}
			}
		}
	}
}
void Update() {
	double time = Time();
	if (IsKey(kKeyLeft) || IsKey("a")) {
		g_dx = -1;
	} else if (IsKey(kKeyRight) || IsKey("d")) {
		g_dx = 1;
	}
	g_rotate = g_rotate || IsKey(kKeyUp) || IsKey("w") || IsKey(" ");
	g_drop = g_drop || IsKey(kKeyDown) || IsKey("s");
	if (IsKey("c")) {
		ClearField();
		StartNewTetramino();
		return;
	}
	if (time - g_prev_time < 0.5) {
		return;
	}
	g_prev_time = time;
	g_dy = 1;
	if (g_dx && IsPositionOk(g_current_x + g_dx, g_current_y, g_current_orientation)) {
		g_current_x += g_dx;
		g_dy = 0;
	} else {
		g_dx = 0;
	}
	if (g_rotate) {
		if (IsPositionOk(g_current_x, g_current_y, (g_current_orientation + 1) % 4)) {
			g_current_orientation = (g_current_orientation + 1) % 4;
			g_dy = 0;
		}
	}
	bool is_lock = false;
	if (IsPositionOk(g_current_x, g_current_y + (g_dy ? 1 : 0), g_current_orientation)) {
		g_current_y += (g_dy ? 1 : 0);
	} else {
		is_lock = (g_dx == 0);
	}
	if (g_drop) {
		while (IsPositionOk(g_current_x, g_current_y + 1, g_current_orientation)) {
			g_current_y++;
		}
	}
	g_dx = 0;
	g_dy = 0;
	g_drop = 0;
	g_rotate = 0;
	if (is_lock) {
		LockTetramino();
		StartNewTetramino();
	}
}
void Render() {
	Clear();
	for (Si32 y = 0; y < 16; ++y) {
		for (Si32 x = 0; x < 8; ++x) {
			g_blocks[g_field[y][x]].Draw(x * 25, (15 - y) * 25);
		}
	}
	for (Si32 y = 0; y < 5; ++y) {
		for (Si32 x = 0; x < 5; ++x) {
			g_blocks[g_current[g_current_orientation][y][x]].Draw(
				(x + g_current_x) * 25, (15 - y - g_current_y) * 25);
		}
	}
	ShowFrame();
}
void EasyMain() {
	Init();
	while (!IsKey(kKeyEscape)) {
		Update();
		Render();
	}
}