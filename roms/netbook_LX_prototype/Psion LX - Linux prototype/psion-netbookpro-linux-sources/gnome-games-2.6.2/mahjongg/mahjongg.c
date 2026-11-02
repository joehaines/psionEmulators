/* -*- Mode: C; indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8 -*- */

/*
 * GNOME-Mahjongg
 * (C) 1998-1999 the Free Software Foundation
 *
 *
 * Author: Francisco Bustamante
 *
 *
 * http://www.nuclecu.unam.mx/~pancho/
 * pancho@nuclecu.unam.mx
 */

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <config.h>

#include <gtk/gtk.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-window-icon.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <games-clock.h>
#include <games-gconf.h>
#include <games-frame.h>

#include "mahjongg.h"
#include "drawing.h"
#include "solubility.h"

#define APPNAME "mahjongg"
#define APPNAME_LONG "Mahjongg"

/* #defines for the tile selection code. */
#define SELECTED_FLAG   1
#define HINT_FLAG       16

/* The number of half-cycles to blink during a hint (less 1) */
#define HINT_BLINK_NUM 5

/* Sorted such that the bottom leftest are first, and layers decrease
 * Bottom left = high y, low x ! */

tilepos easy_map [MAX_TILES] = {
 {13, 7,  4}, {12, 8,  3}, {14, 8,  3}, {12, 6,  3},
 {14, 6,  3}, {10, 10,  2}, {12, 10,  2}, {14, 10,  2},
 {16, 10,  2}, {10, 8,  2}, {12, 8,  2}, {14, 8,  2},
 {16, 8,  2}, {10, 6,  2}, {12, 6,  2}, {14, 6,  2},
 {16, 6,  2}, {10, 4,  2}, {12, 4,  2}, {14, 4,  2},
 {16, 4,  2}, {8, 12,  1}, {10, 12,  1}, {12, 12,  1},
 {14, 12,  1}, {16, 12,  1}, {18, 12,  1}, {8, 10,  1},
 {10, 10,  1}, {12, 10,  1}, {14, 10,  1}, {16, 10,  1}, 
 {18, 10,  1}, {8, 8,  1}, {10, 8,  1}, {12, 8,  1},
 {14, 8,  1}, {16, 8,  1}, {18, 8,  1}, {8, 6,  1},
 {10, 6,  1}, {12, 6,  1}, {14, 6,  1}, {16, 6,  1},
 {18, 6,  1}, {8, 4,  1}, {10, 4,  1}, {12, 4,  1},
 {14, 4,  1}, {16, 4,  1}, {18, 4,  1}, {8, 2,  1},
 {10, 2,  1}, {12, 2,  1}, {14, 2,  1}, {16, 2,  1},
 {18, 2,  1}, {2, 14,  0}, {4, 14,  0}, {6, 14,  0},
 {8, 14,  0}, {10, 14,  0}, {12, 14,  0}, {14, 14,  0}, 
 {16, 14,  0}, {18, 14,  0}, {20, 14,  0}, {22, 14,  0},
 {24, 14,  0}, {6, 12,  0}, {8, 12,  0}, {10, 12,  0},
 {12, 12,  0}, {14, 12,  0}, {16, 12,  0}, {18, 12,  0},
 {20, 12,  0}, {4, 10,  0}, {6, 10,  0}, {8, 10,  0},
 {10, 10,  0}, {12, 10,  0}, {14, 10,  0}, {16, 10,  0},
 {18, 10,  0}, {20, 10,  0}, {22, 10,  0}, {0, 7,  0},
 {2, 8,  0}, {4, 8,  0}, {6, 8,  0}, {8, 8,  0},
 {10, 8,  0}, {12, 8,  0}, {14, 8,  0}, {16, 8,  0},
 {18, 8,  0}, {20, 8,  0}, {22, 8,  0}, {24, 8,  0},
 {2, 6,  0}, {4, 6,  0}, {6, 6,  0}, {8, 6,  0},
 {10, 6,  0}, {12, 6,  0}, {14, 6,  0}, {16, 6,  0},
 {18, 6,  0}, {20, 6,  0}, {22, 6,  0}, {24, 6,  0},
 {4, 4,  0}, {6, 4,  0}, {8, 4,  0}, {10, 4,  0},
 {12, 4,  0}, {14, 4,  0}, {16, 4,  0}, {18, 4,  0},
 {20, 4,  0}, {22, 4,  0}, {6, 2,  0}, {8, 2,  0},
 {10, 2,  0}, {12, 2,  0}, {14, 2,  0}, {16, 2,  0},
 {18, 2,  0}, {20, 2,  0}, {2, 0,  0}, {4, 0,  0},
 {6, 0,  0}, {8, 0,  0}, {10, 0,  0}, {12, 0,  0},
 {14, 0,  0}, {16, 0,  0}, {18, 0,  0}, {20, 0,  0},
 {22, 0,  0}, {24, 0,  0}, {26, 7, 0}, {28, 7, 0} 
};

tilepos hard_map [MAX_TILES] = {
	{ 10, 6,  6},
	{ 9, 6,  5},
	{ 11, 6,  5},
	{ 8, 6,  4},
	{ 10, 6,  4},
	{ 12, 6,  4},
	{ 5, 6,  3},
	{ 7, 7,  3},
	{ 7, 5,  3},
	{ 9, 7,  3},
	{ 9, 5,  3},
	{ 11, 7,  3},
	{ 11, 5,  3},
	{ 13, 7,  3},
	{ 13, 5,  3},
	{ 15, 6,  3},
	{ 5, 8,  2},
	{ 7, 8,  2},
	{ 9, 8,  2},
	{ 11, 8,  2},
	{ 13, 8,  2},
	{ 15, 8,  2},
	{ 4, 6,  2},
	{ 6, 6,  2},
	{ 8, 6,  2},
	{ 10, 6,  2},
	{ 12, 6,  2},
	{ 14, 6,  2},
	{ 16, 6,  2},
	{ 5, 4,  2},
	{ 7, 4,  2},
	{ 9, 4,  2},
	{ 11, 4,  2},
	{ 13, 4,  2},
	{ 15, 4,  2},
	{ 7, 12,  1},
	{ 9, 11,  1},
	{ 11, 11,  1},
	{ 13, 12,  1},
	{ 2, 10,  1},
	{ 4, 10,  1},
	{ 6, 10,  1},
	{ 8, 9,  1},
	{ 10, 9,  1},
	{ 12, 9,  1},
	{ 14, 10,  1},
	{ 16, 10,  1},
	{ 18, 10,  1},
	{ 3, 8,  1},
	{ 3, 6,  1},
	{ 5, 8,  1},
	{ 5, 6,  1},
	{ 7, 7,  1},
	{ 9, 7,  1},
	{ 11, 7,  1},
	{ 13, 7,  1},
	{ 15, 8,  1},
	{ 17, 8,  1},
	{ 3, 4,  1},
	{ 5, 4,  1},
	{ 7, 5,  1},
	{ 9, 5,  1},
	{ 11, 5,  1},
	{ 13, 5,  1},
	{ 15, 6,  1},
	{ 17, 6,  1},
	{ 2, 2,  1},
	{ 4, 2,  1},
	{ 6, 2,  1},
	{ 8, 3,  1},
	{ 10, 3,  1},
	{ 12, 3,  1},
	{ 15, 4,  1},
	{ 17, 4,  1},
	{ 7, 0,  1},
	{ 9, 1,  1},
	{ 11, 1,  1},
	{ 14, 2,  1},
	{ 16, 2,  1},
	{ 18, 2,  1},
	{ 13, 0,  1},
	{ 6, 12,  0},
	{ 8, 12,  0},
	{ 10, 12,  0},
	{ 12, 12,  0},
	{ 14, 12,  0},
	{ 1, 11,  0},
	{ 3, 11,  0},
	{ 1, 9,  0},
	{ 0, 6,  0},
	{ 3, 9,  0},
	{ 5, 10,  0},
	{ 7, 10,  0},
	{ 9, 10,  0},
	{ 11, 10,  0},
	{ 13, 10,  0},
	{ 15, 10,  0},
	{ 17, 11,  0},
	{ 19, 11,  0},
	{ 2, 7,  0},
	{ 4, 7,  0},
	{ 6, 8,  0},
	{ 8, 8,  0},
	{ 2, 5,  0},
	{ 4, 5,  0},
	{ 6, 6,  0},
	{ 8, 6,  0},
	{ 10, 8,  0},
	{ 10, 6,  0},
	{ 12, 8,  0},
	{ 12, 6,  0},
	{ 14, 8,  0},
	{ 14, 6,  0},
	{ 17, 9,  0},
	{ 16, 7,  0},
	{ 19, 9,  0},
	{ 18, 7,  0},
	{ 1, 3,  0},
	{ 3, 3,  0},
	{ 6, 4,  0},
	{ 8, 4,  0},
	{ 10, 4,  0},
	{ 12, 4,  0},
	{ 14, 4,  0},
	{ 16, 5,  0},
	{ 18, 5,  0},
	{ 20, 6,  0},
	{ 1, 1,  0},
	{ 3, 1,  0},
	{ 5, 2,  0},
	{ 7, 2,  0},
	{ 9, 2,  0},
	{ 11, 2,  0},
	{ 13, 2,  0},
	{ 15, 2,  0},
	{ 17, 3,  0},
	{ 19, 3,  0},
	{ 17, 1,  0},
	{ 19, 1,  0},
	{ 6, 0,  0},
	{ 8, 0,  0},
	{ 10, 0,  0},
	{ 12, 0,  0},
	{ 14, 0,  0}
};

tilepos pyramid_map [MAX_TILES] = {
 {13, 14, 6}, {13, 8,  6},  {13, 2, 6}, {12, 14,  5},
 {14, 14,  5}, {12, 8,  5}, {14, 8,  5}, {12, 2,  5},
 {14, 2,  5}, {10, 14,  4}, {12, 14,  4}, {14, 14,  4},
 {16, 14,  4}, {2, 8,  4}, {11, 8,  4}, {13, 8,  4},
 {15, 8,  4}, {24, 8,  4}, {10, 2,  4}, {12, 2,  4},
 {14, 2,  4}, {16, 2,  4}, {8, 14,  3}, {10, 14,  3},
 {12, 14,  3}, {14, 14,  3}, {16, 14,  3}, {18, 14,  3},
 {2, 9,  3}, {24, 9, 3}, {10, 8,  3}, {12, 8,  3},
 {14, 8,  3}, {16, 8, 3}, {2, 7,  3}, {24, 7,  3},
 {8, 2,  3}, {10, 2,  3}, {12, 2,  3}, {14, 2,  3},
 {16, 2,  3}, {18, 2,  3}, {6, 14,  2}, {8, 14,  2},
 {10, 14,  2}, {12, 14,  2}, {14, 14,  2}, {16, 14,  2},
 {18, 14,  2}, {20, 14,  2}, {2, 10,  2}, {24, 10,  2},
 {2, 8,  2}, {9, 8,  2}, {11, 8,  2}, {13, 8,  2},
 {15, 8,  2}, {17, 8,  2}, {24, 8,  2}, {2, 6,  2},
 {24, 6,  2}, {6, 2,  2}, {8, 2,  2}, {10, 2,  2},
 {12, 2,  2}, {14, 2,  2}, {16, 2,  2}, {18, 2,  2},
 {20, 2,  2}, {4, 14,  1}, {6, 14,  1}, {8, 14,  1},
 {10, 14,  1}, {12, 14,  1}, {14, 14,  1}, {16, 14,  1},
 {18, 14,  1}, {20, 14,  1}, {22, 14,  1}, {2, 11,  1},
 {24, 11,  1}, {2, 9,  1}, {24, 9,  1}, {8, 8,  1},
 {10, 8,  1}, {12, 8,  1}, {14, 8,  1}, {16, 8,  1},
 {18, 8,  1}, {2, 7,  1}, {24, 7,  1}, {2, 5,  1},
 {24, 5,  1}, {4, 2,  1}, {6, 2,  1}, {8, 2,  1},
 {10, 2,  1}, {12, 2,  1}, {14, 2,  1}, {16, 2,  1},
 {18, 2,  1}, {20, 2,  1}, {22, 2,  1}, {2, 14,  0},
 {4, 14,  0}, {6, 14,  0}, {8, 14,  0}, {10, 14,  0},
 {12, 14,  0}, {14, 14,  0}, {16, 14,  0}, {18, 14,  0},
 {20, 14,  0}, {22, 14,  0}, {24, 14,  0}, {2, 12,  0},
 {24, 12,  0}, {2, 10,  0}, {24, 10,  0}, {2, 8, 0},
 {7, 8,  0}, {9, 8,  0}, {11, 8,  0}, {13, 8,  0},
 {15, 8,  0}, {17, 8,  0}, {19, 8,  0}, {24, 8, 0},
 {2, 6,  0}, {24, 6,  0}, {2, 4,  0}, {24, 4,  0},
 {2, 2,  0}, {4, 2,  0}, {6, 2,  0}, {8, 2,  0},
 {10, 2,  0}, {12, 2,  0}, {14, 2,  0}, {16, 2,  0},
 {18, 2,  0}, {20, 2,  0}, {22, 2,  0}, {24, 2,  0}
};

tilepos cross_map [MAX_TILES] = {
 {13, 8, 5},
 {13, 12,  4}, {13, 10,  4}, {9, 8,  4}, {11, 8,  4},
 {13, 8,  4}, {15, 8,  4}, {17, 8,  4}, {13, 6,  4},
 {13, 4,  4}, {5, 14,  3}, {21, 14,  3}, {13, 13,  3},
 {13, 11,  3}, {6, 8,  3},
 {8, 8, 3}, {10, 8,  3}, {12, 9,  3}, {12, 7,  3}, {14, 9,  3}, {14, 7,  3},  {16, 8,  3}, {18, 8,  3}, {20, 8, 3},
   {13, 5,  3},
 {13, 3,  3}, {5, 2,  3}, {21, 2,  3}, {5, 14,  2},
 {7, 14,  2}, {13, 14,  2}, {19, 14,  2}, {21, 14,  2},
 {5, 12,  2}, {13, 12,  2}, {21, 12,  2}, {13, 10, 2},
 {5, 8, 2}, {7, 8,  2}, {9, 8,  2}, {11, 8,  2},
 {13, 8,  2}, {15, 8,  2}, {17, 8,  2}, {19, 8,  2},
 {21, 8, 2}, {13, 6, 2}, {5, 4,  2}, {13, 4,  2},
 {21, 4,  2}, {5, 2,  2}, {7, 2,  2}, {13, 2,  2},
 {19, 2,  2}, {21, 2,  2}, {13, 15,  1}, {5, 14,  1},
 {7, 14,  1}, {9, 14,  1}, {17, 14,  1}, {19, 14,  1},
 {21, 14,  1}, {13, 13,  1}, {5, 12,  1}, {7, 12, 1},
 {19, 12, 1}, {21, 12,  1}, {13, 11,  1}, {5, 10,  1},
 {21, 10,  1}, {4, 8,  1},
 {6, 8,  1}, {8, 8,  1}, {10, 8,  1}, {12, 9,  1}, {12, 7,  1},
 {14, 9,  1}, {14, 7,  1},  {16, 8,  1}, {18, 8,  1}, {20, 8,  1},
 {22, 8,  1}, {5, 6,  1}, {21, 6,  1}, {13, 5,  1},
 {5, 4,  1}, {7, 4, 1}, {19, 4, 1}, {21, 4,  1},
 {13, 3,  1}, {5, 2,  1}, {7, 2,  1}, {9, 2,  1},
 {17, 2,  1},{19, 2,  1}, {21, 2,  1}, {13, 1,  1},
 {13, 16,  0}, {5, 14,  0}, {7, 14,  0}, {9, 14,  0},
 {11, 14,  0}, {13, 14,  0}, {15, 14,  0}, {17, 14,  0},
 {19, 14,  0}, {21, 14,  0}, {5, 12,  0}, {7, 12, 0},
 {13, 12,  0}, {19, 12, 0}, {21, 12,  0}, {5, 10,  0},
 {13, 10,  0}, {21, 10,  0}, {3, 8, 0},{5, 8, 0},
 {7, 8,  0}, {9, 8,  0}, {11, 8,  0}, {13, 8,  0},
 {15, 8,  0}, {17, 8,  0}, {19, 8,  0}, {21, 8, 0},
 {23, 8,  0}, {5, 6,  0}, {13, 6,  0}, {21, 6,  0},
 {5, 4,  0}, {7, 4, 0}, {13, 4,  0}, {19, 4, 0},
 {21, 4,  0}, {5, 2,  0}, {7, 2,  0}, {9, 2,  0},
 {11, 2,  0}, {13, 2,  0}, {15, 2,  0}, {17, 2,  0},
 {19, 2,  0}, {21, 2,  0}, {13, 0, 0}
};


tilepos tictactoe_map [MAX_TILES] = {
 {6, 12,  4},{8, 12,  4}, {10, 12,  4}, {12, 12,  4}, {14, 12,  4}, {16, 12, 4},{18, 12,  4},
 {6, 10,  4}, {18, 10,  4},
 {6, 8,  4}, {18, 8,  4},
 {6, 6,  4}, {18, 6,  4},
 {6, 4,  4}, {8, 4,  4}, {10, 4,  4}, {12, 4,  4}, {14, 4,  4}, {16, 4, 4},{18, 4,  4},

 {6, 12,  3}, {8, 12,  3}, {10, 12,  3}, {12, 12,  3}, {14, 12,  3}, {16, 12, 3}, {18, 12,  3},
 {6, 10,  3}, {18, 10,  3},
 {6, 8,  3}, {18, 8,  3},
 {6, 6,  3}, {18, 6,  3},
 {6, 4,  3}, {8, 4,  3}, {10, 4,  3}, {12, 4,  3}, {14, 4,  3}, {16, 4,  3}, {18, 4,  3},

 {6, 14,  2}, {18, 14,  2},
 {4, 12,  2}, {6, 12,  2}, {8, 12,  2}, {10, 12,  2}, {12, 12,  2}, {14, 12, 2}, {16, 12,  2}, {18, 12,  2}, {20, 12,  2},
 {6, 10,  2}, {18, 10,  2},
 {6, 8,  2}, {18, 8,  2},
 {6, 6,  2}, {18, 6,  2},
 {4, 4,  2}, {6, 4,  2}, {8, 4,  2}, {10, 4,  2}, {12, 4,  2}, {14, 4,  2}, {16, 4,  2}, {18, 4,  2}, {20, 4,  2},
 {6, 2,  2}, {18, 2,  2},

 {6, 16,  1}, {18, 16,  1},
 {6, 14,  1}, {18, 14,  1},
 {2, 12,  1}, {4, 12,  1}, {6, 12,  1}, {8, 12,  1}, {10, 12,  1}, {12, 12, 1}, {14, 12,  1}, {16, 12,  1}, {18, 12,  1}, {20, 12,  1}, {22, 12,  1},
 {6, 10,  1}, {18, 10,  1},
 {6, 8,  1}, {18, 8,  1},
 {6, 6,  1}, {18, 6,  1},
 {2, 4,  1}, {4, 4,  1}, {6, 4,  1}, {8, 4,  1}, {10, 4,  1}, {12, 4,  1}, {14, 4,  1}, {16, 4,  1}, {18, 4,  1}, {20, 4,  1}, {22, 4,  1},
 {6, 2,  1}, {18, 2,  1},
 {6, 0,  1}, {18, 0,  1},

 {6, 16,  0}, {18, 16,  0},
 {6, 14,  0}, {18, 14,  0},
 {0, 12, 0}, {2, 12,  0}, {4, 12,  0}, {6, 12,  0}, {8, 12,  0}, {10, 12,  0}, {12, 12,  0}, {14, 12,  0}, {16, 12,  0},{18, 12,  0}, {20, 12,  0},{22, 12, 0}, {24, 12, 0},
 {6, 10,  0}, {18, 10,  0},
 {6, 8,  0}, {18, 8,  0},
 {6, 6,  0}, {18, 6,  0},
 {0, 4, 0}, {2, 4,  0}, {4, 4,  0}, {6, 4,  0}, {8, 4,  0}, {10, 4,  0}, {12, 4,  0}, {14, 4,  0}, {16, 4,  0},{18, 4,  0}, {20, 4,  0},{22, 4,  0}, {24, 4, 0},
 {6, 2,  0}, {18, 2,  0},
 {6, 0,  0}, {18, 0,  0},
};

tilepos cloud_map [MAX_TILES] = {
 {12, 11,  2},

 {0, 6, 2}, {4, 6, 2}, {8, 6, 2}, {12, 6, 2}, {16, 6, 2}, {20, 6, 2}, {24, 6, 2},
 {0, 4, 2}, {4, 4, 2}, {8, 4, 2}, {12, 4, 2}, {16, 4, 2}, {20, 4, 2}, {24, 4, 2},
 {0, 2, 2}, {4, 2, 2}, {8, 2, 2}, {12, 2, 2}, {16, 2, 2}, {20, 2, 2}, {24, 2, 2},
 {0, 0, 2}, {4, 0, 2}, {8, 0, 2}, {12, 0, 2}, {16, 0, 2}, {20, 0, 2}, {24, 0, 2},

 {6, 11, 1}, {8, 11, 1}, {10, 11, 1}, {12, 11, 1}, {14, 11, 1},
 {16, 11, 1}, {18, 11, 1}, {20, 11, 1},

 {0, 6, 1}, {4, 6, 1}, {8, 6, 1}, {12, 6, 1}, {16, 6, 1}, {20, 6, 1}, {24, 6, 1},
 {0, 4, 1}, {4, 4, 1}, {8, 4, 1}, {12, 4, 1}, {16, 4, 1}, {20, 4, 1}, {24, 4, 1},
 {0, 2, 1}, {4, 2, 1}, {8, 2, 1}, {12, 2, 1}, {16, 2, 1}, {20, 2, 1}, {24, 2, 1},
 {0, 0, 1}, {4, 0, 1}, {8, 0, 1}, {12, 0, 1}, {16, 0, 1}, {20, 0, 1}, {24, 0, 1},

 {5, 11, 0}, {7, 11, 0}, {9, 11, 0}, {11, 11, 0}, {13, 11, 0},
 {15, 11, 0}, {17, 11, 0}, {19, 11, 0}, {21, 11, 0},

 {0, 8, 0}, {2, 8, 0}, {4, 8, 0}, {6, 8, 0}, {8, 8, 0}, {10, 8, 0}, {12, 8, 0},
 {14, 8, 0}, {16, 8, 0}, {18, 8, 0}, {20, 8, 0}, {22, 8, 0}, {24, 8, 0}, {26, 8, 0},

 {0, 6, 0}, {2, 6, 0}, {4, 6, 0}, {6, 6, 0}, {8, 6, 0}, {10, 6, 0}, {12, 6, 0},
 {14, 6, 0}, {16, 6, 0}, {18, 6, 0}, {20, 6, 0}, {22, 6, 0}, {24, 6, 0}, {26, 6, 0},

 {0, 4, 0}, {2, 4, 0}, {4, 4, 0}, {6, 4, 0}, {8, 4, 0}, {10, 4, 0}, {12, 4, 0},
 {14, 4, 0}, {16, 4, 0}, {18, 4, 0}, {20, 4, 0}, {22, 4, 0}, {24, 4, 0}, {26, 4, 0},

 {0, 2, 0}, {2, 2, 0}, {4, 2, 0}, {6, 2, 0}, {8, 2, 0}, {10, 2, 0}, {12, 2, 0},
 {14, 2, 0}, {16, 2, 0}, {18, 2, 0}, {20, 2, 0}, {22, 2, 0}, {24, 2, 0}, {26, 2, 0},

 {0, 0, 0}, {2, 0, 0}, {4, 0, 0}, {6, 0, 0}, {8, 0, 0}, {10, 0, 0}, {12, 0, 0},
 {14, 0, 0}, {16, 0, 0}, {18, 0, 0}, {20, 0, 0}, {22, 0, 0}, {24, 0, 0}, {26, 0, 0},

};

tilepos reddragon_map [MAX_TILES] = {
{10,8,2},
{12,8,2},
{14,8,2},
{16,8,2},
{22,8,2},
{10,6,2},
{12,6,2},
{14,6,2},
{16,6,2},
{10,4,2},
{12,4,2},
{14,4,2},
{16,4,2},
{10,2,2},
{12,2,2},
{14,2,2},
{16,2,2},
{5,9,1},
{7,9,1},
{9,9,1},
{11,9,1},
{13,9,1},
{15,9,1},
{17,9,1},
{19,9,1},
{21,9,1},
{7,7,1},
{9,7,1},
{11,7,1},
{13,7,1},
{15,7,1},
{17,7,1},
{19,7,1},
{21,7,1},
{23,7,1},
{7,5,1},
{9,5,1},
{11,5,1},
{13,5,1},
{15,5,1},
{17,5,1},
{19,5,1},
{21,5,1},
{23,5,1},
{5,3,1},
{7,3,1},
{9,3,1},
{11,3,1},
{13,3,1},
{15,3,1},
{17,3,1},
{19,3,1},
{21,3,1},
{23,3,1},
{7,1,1},
{9,1,1},
{11,1,1},
{13,1,1},
{15,1,1},
{17,1,1},
{19,1,1},
{21,1,1},
{4,13,0},
{8,13,0},
{12,13,0},
{16,13,0},
{20,13,0},
{24,13,0},
{0,12,0},
{28,12,0},
{4,10,0},
{6,10,0},
{8,10,0},
{10,10,0},
{12,10,0},
{14,10,0},
{16,10,0},
{18,10,0},
{20,10,0},
{22,10,0},
{24,10,0},
{0,9,0},
{28,9,0},
{4,8,0},
{6,8,0},
{8,8,0},
{10,8,0},
{12,8,0},
{14,8,0},
{16,8,0},
{18,8,0},
{20,8,0},
{22,8,0},
{24,8,0},
{0,6,0},
{4,6,0},
{6,6,0},
{8,6,0},
{10,6,0},
{12,6,0},
{14,6,0},
{16,6,0},
{18,6,0},
{20,6,0},
{22,6,0},
{24,6,0},
{28,6,0},
{4,4,0},
{6,4,0},
{8,4,0},
{10,4,0},
{12,4,0},
{14,4,0},
{16,4,0},
{18,4,0},
{20,4,0},
{22,4,0},
{24,4,0},
{0,3,0},
{28,3,0},
{4,2,0},
{6,2,0},
{8,2,0},
{10,2,0},
{12,2,0},
{14,2,0},
{16,2,0},
{18,2,0},
{20,2,0},
{22,2,0},
{24,2,0},
{0,0,0},
{4,0,0},
{6,0,0},
{8,0,0},
{10,0,0},
{12,0,0},
{14,0,0},
{16,0,0},
{18,0,0},
{20,0,0},
{22,0,0},
{24,0,0},
{28,0,0}
};
tilepos fourbridges_map [MAX_TILES] = {
       {7,13,3},
       {17,13,3},
       {7,3,3},
       {17,3,3},
       {6,14,2},
       {8,14,2},
       {16,14,2},
       {18,14,2},
       {6,12,2},
       {8,12,2},
       {16,12,2},
       {18,12,2},
       {6,4,2},
       {8,4,2},
       {16,4,2},
       {18,4,2},
       {6,2,2},
       {8,2,2},
       {16,2,2},
       {18,2,2},
       {5,15,1},
       {7,15,1},
       {9,15,1},
       {15,15,1},
       {17,15,1},
       {19,15,1},
       {5,13,1},
       {7,13,1},
       {9,13,1},
       {15,13,1},
       {17,13,1},
       {19,13,1},
       {5,11,1},
       {7,11,1},
       {9,11,1},
       {15,11,1},
       {17,11,1},
       {19,11,1},
       {5,5,1},
       {7,5,1},
       {9,5,1},
       {15,5,1},
       {17,5,1},
       {19,5,1},
       {5,3,1},
       {7,3,1},
       {9,3,1},
       {15,3,1},
       {17,3,1},
       {19,3,1},
       {5,1,1},
       {7,1,1},
       {9,1,1},
       {15,1,1},
       {17,1,1},
       {19,1,1},
       {2,16,0},
       {4,16,0},
       {6,16,0},
       {8,16,0},
       {10,16,0},
       {12,16,0},
       {14,16,0},
       {16,16,0},
       {18,16,0},
       {20,16,0},
       {22,16,0},
       {4,14,0},
       {6,14,0},
       {8,14,0},
       {10,14,0},
       {14,14,0},
       {16,14,0},
       {18,14,0},
       {20,14,0},
       {4,12,0},
       {6,12,0},
       {8,12,0},
       {10,12,0},
       {12,12,0},
       {14,12,0},
       {16,12,0},
       {18,12,0},
       {20,12,0},
       {0,10,0},
       {2,10,0},
       {4,10,0},
       {6,10,0},
       {8,10,0},
       {10,10,0},
       {12,10,0},
       {14,10,0},
       {16,10,0},
       {18,10,0},
       {20,10,0},
       {22,10,0},
       {24,10,0},
       {3,8,0},
       {5,8,0},
       {7,8,0},
       {17,8,0},
       {19,8,0},
       {21,8,0},
       {0,6,0},
       {2,6,0},
       {4,6,0},
       {6,6,0},
       {8,6,0},
       {10,6,0},
       {12,6,0},
       {14,6,0},
       {16,6,0},
       {18,6,0},
       {20,6,0},
       {22,6,0},
       {24,6,0},
       {4,4,0},
       {6,4,0},
       {8,4,0},
       {10,4,0},
       {12,4,0},
       {14,4,0},
       {16,4,0},
       {18,4,0},
       {20,4,0},
       {4,2,0},
       {6,2,0},
       {8,2,0},
       {10,2,0},
       {14,2,0},
       {16,2,0},
       {18,2,0},
       {20,2,0},
       {2,0,0},
       {4,0,0},
       {6,0,0},
       {8,0,0},
       {10,0,0},
       {12,0,0},
       {14,0,0},
       {16,0,0},
       {18,0,0},
       {20,0,0},
       {22,0,0}
};


tilepos *pos = 0;

tile tiles[MAX_TILES];

GtkWidget *window, *appbar;
GtkWidget *tiles_label;
gint selected_tile, visible_tiles;
gint sequence_number;

guint current_seed, next_seed;

gint windowwidth, windowheight;

GList * tileset_list = NULL;

gchar *tileset = NULL;
static gchar *mapset = NULL;
static gchar *score_current_mapset = NULL;

static gchar *selected_tileset = NULL;

struct _maps
{
  gchar *name ;
  gchar *score_name;
  tilepos *map ;
} maps[] = {
	{ N_("Easy"), "easy", easy_map },
	{ N_("Difficult"), "difficult", hard_map },
        { N_("Confounding Cross"), "confounding", cross_map },
        { N_("Pyramid's Walls"), "pyramid", pyramid_map },
        { N_("Tic-Tac-Toe"), "tictactoe", tictactoe_map },
        { N_("Cloud"), "cloud", cloud_map },
        { N_("Red Dragon"), "dragon", reddragon_map },
        { N_("Four Bridges"), "bridges", fourbridges_map },

};


gint hint_tiles[2];
guint timer;
guint timeout_counter = HINT_BLINK_NUM + 1;

GtkWidget *moves_label;
gint moves_left=0;
GtkWidget *chrono;
gint paused=0;

/* for the preferences */
GConfClient *conf_client;
gboolean popup_warn = FALSE;
GtkWidget *warn_cb = NULL, *confirm_cb = NULL;
GtkWidget *colour_well = NULL;
GtkWidget *pref_dialog = NULL;
GtkWidget *about = NULL;

/* Has the map been changed ? */
gboolean new_map = TRUE;

typedef enum {
	NEW_GAME,
	NEW_GAME_WITH_SEED,
	RESTART_GAME,
	QUIT_GAME
} game_state;

enum {
	GAME_RUNNING,
	GAME_WON,
	GAME_LOST,
	GAME_DEAD
} game_over;

static void clear_undo_queue (void);
void you_won (void);
void no_match (void);
void check_free (void);
void undo_tile_callback    (GtkWidget *widget, gpointer data);
void redo_tile_callback    (GtkWidget *widget, gpointer data);
void hint_callback         (GtkWidget *widget, gpointer data);
void properties_callback   (GtkWidget *widget, gpointer data);
void about_callback        (GtkWidget *widget, gpointer data);
void show_tb_callback      (GtkWidget *widget, gpointer data);
void sound_on_callback     (GtkWidget *widget, gpointer data);
void scores_callback       (GtkWidget *widget, gpointer data);
gboolean delete_event_callback (GtkWidget *widget, GdkEventAny *any, gpointer data);
void confirm_action       (GtkWidget *widget, gpointer data);
void shuffle_tiles_callback   (GtkWidget *widget, gpointer data);
void ensure_pause_off (void);
void pause_callback (void);
void new_game (gboolean with_seed);
void restart_game (void);
void select_game (GtkWidget *widget, gpointer data);

GnomeUIInfo gamemenu [] = {
         GNOMEUIINFO_MENU_NEW_GAME_ITEM(confirm_action, NEW_GAME),

         {GNOME_APP_UI_ITEM, N_("New game with _seed..."),
		 N_("Start a new game giving a seed number..."),
		 select_game, NULL, NULL, GNOME_APP_PIXMAP_STOCK,
		 GTK_STOCK_NEW, 0, 0, NULL},

	 GNOMEUIINFO_MENU_RESTART_GAME_ITEM(confirm_action, RESTART_GAME),

	 GNOMEUIINFO_MENU_PAUSE_GAME_ITEM (pause_callback, NULL),
	 
	 GNOMEUIINFO_SEPARATOR,

	 GNOMEUIINFO_MENU_UNDO_MOVE_ITEM(undo_tile_callback, NULL),
	 GNOMEUIINFO_MENU_REDO_MOVE_ITEM(redo_tile_callback, NULL),

	 GNOMEUIINFO_MENU_HINT_ITEM(hint_callback, NULL),

         {GNOME_APP_UI_ITEM, N_("Shu_ffle tiles"), N_("Shuffle tiles"),
		 shuffle_tiles_callback, NULL, NULL, GNOME_APP_PIXMAP_STOCK,
		 NULL, 0, 0, NULL},

	 GNOMEUIINFO_SEPARATOR,

         GNOMEUIINFO_MENU_SCORES_ITEM(scores_callback, NULL),

	 GNOMEUIINFO_SEPARATOR,
         GNOMEUIINFO_MENU_QUIT_ITEM(confirm_action, QUIT_GAME),

	 GNOMEUIINFO_END
};

GnomeUIInfo settingsmenu [] = {
        GNOMEUIINFO_TOGGLEITEM(N_("_Toolbar"),
			       N_("Show or hide the toolbar"),
			       show_tb_callback, NULL),

	GNOMEUIINFO_MENU_PREFERENCES_ITEM(properties_callback, NULL),
	GNOMEUIINFO_END
};

GnomeUIInfo helpmenu[] = {
        GNOMEUIINFO_HELP("mahjongg"),
	GNOMEUIINFO_MENU_ABOUT_ITEM(about_callback, NULL),
	GNOMEUIINFO_END

};

GnomeUIInfo mainmenu [] = {
	GNOMEUIINFO_MENU_GAME_TREE(gamemenu),
	GNOMEUIINFO_MENU_SETTINGS_TREE(settingsmenu),
	GNOMEUIINFO_MENU_HELP_TREE(helpmenu),
	GNOMEUIINFO_END
};

GnomeUIInfo toolbar_uiinfo [] = {
	{GNOME_APP_UI_ITEM, N_("New"), N_("New game"), confirm_action,
		(gpointer)NEW_GAME, NULL,
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_NEW, 0, 0, NULL},

        {GNOME_APP_UI_ITEM, N_("Restart"), N_("Restart game"), confirm_action,
		(gpointer)RESTART_GAME, NULL,
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_REFRESH, 0, 0, NULL},

        /* If you change the place for this button, change the index in
           the definition of PAUSE_BUTTON below */
        {GNOME_APP_UI_TOGGLEITEM, N_("Pause"), N_("Pause game"),
		pause_callback, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_STOP, 0, 0, NULL},

	GNOMEUIINFO_SEPARATOR,
	
        {GNOME_APP_UI_ITEM, N_("Undo"), N_("Undo previous move"),
		undo_tile_callback, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_UNDO, 0, 0, NULL},

        {GNOME_APP_UI_ITEM, N_("Redo"), N_("Redo"), redo_tile_callback,
		NULL, NULL, GNOME_APP_PIXMAP_STOCK, GTK_STOCK_REDO, 0, 0, NULL},
        {GNOME_APP_UI_ITEM, N_("Hint"), N_("Get a hint"), hint_callback,
		NULL, NULL, GNOME_APP_PIXMAP_STOCK, GTK_STOCK_HELP, GDK_H,
		GDK_CONTROL_MASK, NULL},

	{GNOME_APP_UI_ITEM, N_("Shuffle"), N_("Shuffle tiles"),
		shuffle_tiles_callback, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE, 0, 0, NULL},

	{GNOME_APP_UI_ENDOFINFO}
};

#define PAUSE_BUTTON GTK_TOGGLE_BUTTON(toolbar_uiinfo[2].widget)
#define HIGHSCORE_WIDGET gamemenu[10].widget

/* At the end of the game, hint, shuffle and pause all become unavailable. */
/* Undo and Redo are handled elsewhere. */
static void set_menus_sensitive (void)
{
	gboolean state;
	
	/* Pause */
	state = game_over != GAME_WON;
	gtk_widget_set_sensitive (gamemenu[3].widget, state);
	gtk_widget_set_sensitive (toolbar_uiinfo[2].widget, state);

	/* Hint */
	state = moves_left > 0;
	gtk_widget_set_sensitive (gamemenu[7].widget, state);
	gtk_widget_set_sensitive (toolbar_uiinfo[6].widget, state);

	/* Shuffle */
	state = game_over != GAME_WON;
	gtk_widget_set_sensitive (gamemenu[5].widget, state);
	gtk_widget_set_sensitive (toolbar_uiinfo[6].widget, state);

	/* Restart */
	state = sequence_number != 1;
	gtk_widget_set_sensitive (gamemenu[2].widget, state);
	gtk_widget_set_sensitive (toolbar_uiinfo[1].widget, state);
}

/* Undo and redo sensitivity functionality. */
static void set_undoredo_sensitive (gboolean undo, gboolean redo)
{
	gtk_widget_set_sensitive(gamemenu[5].widget, undo);
	gtk_widget_set_sensitive(toolbar_uiinfo[4].widget, undo);
	gtk_widget_set_sensitive(gamemenu[6].widget, redo);
	gtk_widget_set_sensitive(toolbar_uiinfo[5].widget, redo);
}

static void
tileset_callback (GtkWidget *widget, void *data)
{
	GList * entry;
	
	entry = g_list_nth (tileset_list,
			    gtk_combo_box_get_active (GTK_COMBO_BOX (widget)));	
	
	gconf_client_set_string (conf_client,
			"/apps/mahjongg/tileset",
			entry->data,
			NULL);
}

static void
tileset_changed_cb (GConfClient *client,
		    guint cnxn_id,
		    GConfEntry *entry,
		    gpointer user_data)
{
	char *tile_tmp = NULL;

	tile_tmp = gconf_client_get_string (conf_client,
			"/apps/mahjongg/tileset", NULL);
	if (tile_tmp) {
		if (strcmp (tile_tmp, selected_tileset) != 0) {
			g_free (selected_tileset);
			selected_tileset = tile_tmp;
			load_images (selected_tileset);
			draw_all_tiles ();
		} else {
			g_free (tile_tmp);
		}
	}
}

static void
popup_warn_callback (GtkWidget *widget, gpointer data)
{
	popup_warn = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON (widget));

	gconf_client_set_bool (conf_client,
			"/apps/mahjongg/warn", popup_warn, NULL);
}

static void
popup_warn_changed_cb (GConfClient *client,
		guint cnxn_id,
		GConfEntry *entry,
		gpointer user_data)
{
	gboolean popup_warn_tmp;

	popup_warn_tmp = gconf_client_get_bool (conf_client,
			"/apps/mahjongg/warn", NULL);
	if (popup_warn_tmp != popup_warn)
	{
		popup_warn = popup_warn_tmp;
		if (warn_cb != NULL)
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON (warn_cb),
				popup_warn);
	}
}

static void
show_toolbar_changed_cb (GConfClient *client,
		guint cnxn_id,
		GConfEntry *entry,
		gpointer user_data)
{
	BonoboDockItem *gdi;
	gboolean shown;

	shown = gconf_client_get_bool (conf_client,
			"/apps/mahjongg/show_toolbar", NULL);

	gdi = gnome_app_get_dock_item_by_name (GNOME_APP (window),
			GNOME_APP_TOOLBAR_NAME);

	if (shown == TRUE)
	{
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(settingsmenu[0].widget), TRUE);
		gtk_widget_show(GTK_WIDGET(gdi));
	} else {
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(settingsmenu[0].widget), FALSE);
		gtk_widget_hide(GTK_WIDGET(gdi));
		gtk_widget_queue_resize (window);
	}
}

void
show_tb_callback (GtkWidget *widget, gpointer data)
{
	if ((GTK_CHECK_MENU_ITEM (widget))->active) {
		gconf_client_set_bool(conf_client,
				"/apps/mahjongg/show_toolbar", TRUE, NULL);
	} else {
		gconf_client_set_bool(conf_client,
				"/apps/mahjongg/show_toolbar", FALSE, NULL);
	}
}

static void
bg_colour_changed_cb (GConfClient *client,
		      guint cnxn_id,
		      GConfEntry *entry,
		      gpointer user_data)
{
	gchar *colour;
	
	colour = gconf_client_get_string (conf_client,
			"/apps/mahjongg/bgcolour", NULL);
	set_background (colour);
	if (colour_well != NULL)
	{
		gtk_color_button_set_color (GTK_COLOR_BUTTON(colour_well),
					    &bgcolour);
	}
	draw_all_tiles ();	
}

static void
bg_colour_callback (GtkWidget *widget, gpointer data)
{
	static char *tmp = "";
	GdkColor colour;

	gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colour);

	tmp = g_strdup_printf ("#%04x%04x%04x", colour.red,
			       colour.green, colour.blue);

	gconf_client_set_string (conf_client,
			"/apps/mahjongg/bgcolour", tmp, NULL);
}

static void
mapset_changed_cb (GConfClient *client,
		   guint        cnxn_id,
		   GConfEntry  *entry,
		   gpointer     user_data)
{
	GtkWidget *dialog;
	char *mapset_tmp;

	mapset_tmp = gconf_client_get_string (conf_client,
			"/apps/mahjongg/mapset",
			NULL);
	/* We check whether the name is valid later. */
	if (mapset_tmp != NULL) {
		g_free (mapset);
		mapset = mapset_tmp;
	} 

	new_map = TRUE;
	
	dialog = gtk_message_dialog_new (
		GTK_WINDOW (window),
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		_("This new mapset will take effect when you start "
		  "a new game, or when Mahjongg is restarted."));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
set_map_selection (GtkWidget *widget, void *data)
{
	struct _maps *map;

	map = &(maps[gtk_combo_box_get_active (GTK_COMBO_BOX (widget))]);
	
	g_free (mapset);
	mapset = g_strdup (map->name);

	gconf_client_set_string (conf_client,
				 "/apps/mahjongg/mapset",
				 mapset, NULL);
}

static void
init_config (void)
{
	gconf_client_add_dir (conf_client,
			"/apps/mahjongg", GCONF_CLIENT_PRELOAD_ONELEVEL,
			NULL);
	gconf_client_notify_add (conf_client,
			"/apps/mahjongg/show_toolbar",
			show_toolbar_changed_cb,
			NULL, NULL, NULL);
	gconf_client_notify_add (conf_client,
			"/apps/mahjongg/bgcolour",
			bg_colour_changed_cb,
			NULL, NULL, NULL);
	gconf_client_notify_add (conf_client,
			"/apps/mahjongg/mapset",
			mapset_changed_cb,
			NULL, NULL, NULL);
	gconf_client_notify_add (conf_client,
			"/apps/mahjongg/warn",
			popup_warn_changed_cb,
			NULL, NULL, NULL);
	gconf_client_notify_add (conf_client,
			"/apps/mahjongg/tileset",
			tileset_changed_cb,
			NULL, NULL, NULL);
}

static void
message (gchar *message)
{
	gnome_appbar_pop (GNOME_APPBAR (appbar));
	gnome_appbar_push (GNOME_APPBAR (appbar), message);
}

static void 
update_score_state ()
{
        gchar **names = NULL;
        gfloat *scores = NULL;
        time_t *scoretimes = NULL;
	gint top;

	top = gnome_score_get_notable (APPNAME, score_current_mapset,
				       &names, &scores, &scoretimes);
	gtk_widget_set_sensitive (HIGHSCORE_WIDGET, top > 0);
	g_strfreev (names);
	g_free (scores);
	g_free (scoretimes);
}


static void
chrono_start (void)
{
	games_clock_stop (GAMES_CLOCK (chrono));
	games_clock_set_seconds (GAMES_CLOCK (chrono), 0);
	games_clock_start (GAMES_CLOCK (chrono));
}

static gint
update_moves_left (void)
{
        char *tmpstr;

        check_free ();
	tmpstr = g_strdup_printf ("%2d", moves_left);
        gtk_label_set_text (GTK_LABEL (moves_label), tmpstr);
	g_free (tmpstr);

        return moves_left;
}

static void
select_tile (gint tileno)
{
        tiles[tileno].selected |= SELECTED_FLAG;
        draw_tile (tileno);
        selected_tile = tileno;
}

static void
unselect_tile (gint tileno)
{
        selected_tile = MAX_TILES + 1;
        tiles[tileno].selected &= ~SELECTED_FLAG;
        draw_tile (tileno);
}

static void
remove_pair (gint tile1, gint tile2)
{
	gchar * tmpstr;
	
	tiles[tile1].visible = tiles[tile2].visible = 0;
	tiles[tile1].selected &= ~SELECTED_FLAG;
	tiles[tile2].selected &= ~SELECTED_FLAG;
	draw_tile (tile1);
	draw_tile (tile2);
	clear_undo_queue ();
	tiles[tile1].sequence = tiles[tile2].sequence = sequence_number;
	sequence_number ++;
	selected_tile = MAX_TILES + 1;
	visible_tiles -= 2;
	tmpstr = g_strdup_printf("%3d", visible_tiles);
	gtk_label_set_text (GTK_LABEL(tiles_label), tmpstr);
	g_free (tmpstr);
	update_moves_left ();
	set_undoredo_sensitive (TRUE, FALSE);
	set_menus_sensitive ();
	
	if (visible_tiles <= 0) {
		games_clock_stop(GAMES_CLOCK(chrono));
		you_won ();
	}

}

void
tile_event (gint tileno, gint button)
{
	if (paused)
		return;

	if (!tile_free (tileno))
		return;
	
	switch (button) {
	case 1:
		if (tiles[tileno].selected & SELECTED_FLAG) {
			unselect_tile (tileno);
			return;
		}
		if (selected_tile >= MAX_TILES) {
			select_tile (tileno);
			return;
		}
		if ((tiles[selected_tile].type == tiles[tileno].type) ) {
			remove_pair (selected_tile, tileno);
			return;
		}
		no_match ();
	break;
	
	case 3:
		if (selected_tile < MAX_TILES) 
			unselect_tile (selected_tile);
		select_tile (tileno);
		
	default: 
		break;
	}
}

static void
fill_tile_menu (GtkWidget *menu, gchar *sdir)
{
	struct dirent *e;
	DIR *dir;
        gint itemno = 0;
	gchar *dname = NULL;

	if (tileset_list) {
		g_list_foreach (tileset_list,
				(GFunc) g_free,
				NULL);
		g_list_free (tileset_list);
	}

	tileset_list = NULL;
	
	dname = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
			(sdir), FALSE, NULL);
	dir = opendir (dname);

	if (!dir) {
		g_free (dname);
		return;
	}

	while ((e = readdir (dir)) != NULL){
		gchar *s = g_strdup (e->d_name);

		if (!(g_strrstr (s, ".xpm") ||
		      g_strrstr (s, ".svg") ||
		      g_strrstr (s, ".gif") ||
		      g_strrstr (s, ".png") ||
		      g_strrstr (s, ".jpg") ||
		      g_strrstr (s, ".xbm"))){
			g_free (s);
			continue;
		}

		gtk_combo_box_append_text (GTK_COMBO_BOX (menu), s);
		tileset_list = g_list_append (tileset_list, s);
		if (!strcmp(tileset, s)) {
			gtk_combo_box_set_active(GTK_COMBO_BOX (menu), itemno);
		}
		
	        itemno++;
	}

	closedir (dir);
	g_free (dname);
}

static void
fill_map_menu (GtkWidget *menu)
{
	gint lp, itemno=0 ;

	for (lp=0;lp<G_N_ELEMENTS(maps);lp++) {
		gchar *str = g_strdup(_(maps[lp].name)) ;

		gtk_combo_box_append_text (GTK_COMBO_BOX (menu), str) ;
		if (!g_ascii_strcasecmp (mapset, maps[lp].name))
			gtk_combo_box_set_active (GTK_COMBO_BOX (menu), itemno); 
		itemno++ ;
	}
}

void
no_match (void)
{
	if (popup_warn == TRUE) {
		GtkWidget *mb;

		mb = gtk_message_dialog_new (GTK_WINDOW (window),
				GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_OK,
				_("These tiles don't match."));
		gtk_dialog_run (GTK_DIALOG (mb));
		gtk_widget_destroy (mb);
        } else
		gnome_app_flash (GNOME_APP (window),
				_("These tiles don't match."));
}

void
check_free (void)
{
	gint i;
        gint tile_count[MAX_TILES];

        moves_left = 0;

        for (i=0; i<MAX_TILES; i++)
                tile_count[i] = 0;

	for (i=0;i<MAX_TILES;i++) {
                if (tile_free(i))
                        tile_count[tiles[i].type]++;
        }

        for (i=0; i<MAX_TILES; i++)
                moves_left += tile_count[i]>>1;

 	if ((moves_left == 0) && (visible_tiles>0)) { 
                GtkWidget *mb;

		set_menus_sensitive ();
                if (!game_over) {
			mb = gtk_message_dialog_new (GTK_WINDOW (window),
						     GTK_DIALOG_MODAL
						     | GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_INFO,
						     GTK_BUTTONS_NONE,
						     (_("There are no more moves.")));
			gtk_dialog_add_buttons (GTK_DIALOG (mb),
						_("Shuffle"),
						GTK_RESPONSE_REJECT,
						GTK_STOCK_OK,
						GTK_RESPONSE_ACCEPT,
						NULL);
			gtk_dialog_set_default_response (GTK_DIALOG (mb),
							 GTK_RESPONSE_ACCEPT);
			if (gtk_dialog_run (GTK_DIALOG (mb)) == GTK_RESPONSE_REJECT)
				shuffle_tiles_callback (NULL, NULL);
			gtk_widget_destroy (mb);
                }
                game_over = GAME_LOST;
 	} 
}

void
you_won (void)
{
        gint pos;
        time_t seconds;
        gfloat score;
        GtkWidget *dialog;
	
        game_over = GAME_WON;

        seconds = GAMES_CLOCK (chrono)->stopped;

        score = (seconds / 60) * 1.0 + (seconds % 60) / 100.0;
	pos = gnome_score_log (score, score_current_mapset, FALSE);
	set_menus_sensitive ();
        if (pos) {
                dialog = gnome_scores_display (_(APPNAME_LONG), APPNAME,
					       score_current_mapset, pos);
		if (dialog != NULL) {
			gtk_window_set_transient_for (GTK_WINDOW(dialog),
						      GTK_WINDOW(window));
			gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
		}
        } 
}

static void
pref_dialog_response (GtkDialog *dialog, gint response, gpointer data)
{
	gtk_widget_destroy (pref_dialog);
	pref_dialog = NULL;
	warn_cb = NULL;
	confirm_cb = NULL;
	colour_well = NULL;
}

void
properties_callback (GtkWidget *widget, gpointer data)
{
	GtkWidget *omenu;
	GtkWidget *frame, *table, *w, *label;
	GtkSizeGroup *group;
	GtkWidget *top_table;

	if (pref_dialog) {
		gtk_window_present (GTK_WINDOW (pref_dialog));
		return;
	}

	pref_dialog = gtk_dialog_new_with_buttons (_("Preferences"),
			GTK_WINDOW (window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (pref_dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (pref_dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (pref_dialog),
					 GTK_RESPONSE_CLOSE);
	g_signal_connect (G_OBJECT (pref_dialog), "response",
			  G_CALLBACK (pref_dialog_response), NULL);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	top_table = gtk_table_new (2, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (top_table), 0);
	gtk_table_set_row_spacings (GTK_TABLE (top_table), 0);
	gtk_table_set_col_spacings (GTK_TABLE (top_table), 0);

	frame = games_frame_new (_("Tiles"));
	gtk_table_attach_defaults (GTK_TABLE (top_table), frame, 0, 1, 0, 1);

	table = gtk_table_new (2, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	
	label = gtk_label_new (_("Tile Set"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_size_group_add_widget (group, label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	omenu = gtk_combo_box_new_text ();
	fill_tile_menu (omenu, "mahjongg");
	g_signal_connect (G_OBJECT (omenu), "changed",
			  G_CALLBACK (tileset_callback), NULL); 
	gtk_table_attach_defaults (GTK_TABLE (table), omenu, 1, 2, 0, 1);

	gtk_container_add(GTK_CONTAINER (frame), table);

	frame = games_frame_new (_("Maps"));
	gtk_table_attach_defaults (GTK_TABLE (top_table), frame, 0, 1, 1, 2);

	table = gtk_table_new (1, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	
	label = gtk_label_new (_("Select Map"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_size_group_add_widget (group, label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	omenu = gtk_combo_box_new_text ();
	fill_map_menu (omenu);
	g_signal_connect (G_OBJECT (omenu), "changed",
			  G_CALLBACK (set_map_selection), NULL); 
	gtk_table_attach_defaults (GTK_TABLE (table), omenu, 1, 2, 0, 1);

	gtk_container_add(GTK_CONTAINER (frame), table);

	frame = games_frame_new (_("Colors"));
	gtk_table_attach_defaults (GTK_TABLE (top_table), frame, 1, 2, 0, 1);

	table = gtk_table_new (1, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	
	label = gtk_label_new (_("Background Color"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_size_group_add_widget (group, label);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	w  = gtk_color_button_new ();
	gtk_color_button_set_color (GTK_COLOR_BUTTON (w), &bgcolour);
	g_signal_connect (G_OBJECT (w), "color_set",
			  G_CALLBACK (bg_colour_callback), NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), w, 1, 2, 0, 1);

	gtk_container_add (GTK_CONTAINER (frame), table);


	frame = games_frame_new (_("Warnings"));
	gtk_table_attach_defaults (GTK_TABLE (top_table), frame, 1, 2, 1, 2);

	table = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);

	w = gtk_check_button_new_with_label (_("Warn when tiles don't match"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), popup_warn);
	g_signal_connect (G_OBJECT(w), "clicked", G_CALLBACK (popup_warn_callback), NULL);
	gtk_box_pack_start_defaults (GTK_BOX (table), w);
	
	gtk_container_add(GTK_CONTAINER (frame), table);

	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (pref_dialog)->vbox), top_table);

	g_object_unref (group);
	gtk_widget_show_all (pref_dialog);
}

static gint
hint_timeout (gpointer data)
{
	timeout_counter ++;

	if (timeout_counter > HINT_BLINK_NUM) {
		if (selected_tile < MAX_TILES)
			tiles[selected_tile].selected = 1;
		return 0;
	}

	tiles[hint_tiles[0]].selected ^= HINT_FLAG;
	tiles[hint_tiles[1]].selected ^= HINT_FLAG;
	draw_tile (hint_tiles[0]);
	draw_tile (hint_tiles[1]);
	
	return 1;
}

void
hint_callback (GtkWidget *widget, gpointer data)
{
        gint i, j, free=0, type ;
        time_t seconds;

        if (paused || game_over)
                return;

	/* This prevents the flashing speeding up if the hint button is
	 * pressed multiple times. */
	if (timeout_counter <= HINT_BLINK_NUM)
		return;

	/* Snarfed from check free
	 * Tile Free is now _so_ much quicker, it is more elegant to do a
	 * British Library search, and safer. */

	/* Clear any selection */
	if (selected_tile < MAX_TILES) {
		tiles[selected_tile].selected = 0;
		draw_tile (selected_tile);
		selected_tile = MAX_TILES + 1;
	}
                
	for (i=0;i<MAX_TILES && !free;i++)
		if (tile_free(i)) {
			type = tiles[i].type ;
			for (j=0;j<MAX_TILES && !free;j++) {
				free = (tiles[j].type == type && i != j && tile_free(j)) ;
				if (free) {
					tiles[i].selected ^= HINT_FLAG;
					tiles[j].selected ^= HINT_FLAG;
					draw_tile (i);
					draw_tile (j);
					hint_tiles[0] = i;
					hint_tiles[1] = j;
				}
			}
		}

	/* This is a good way to test check_free
	   for (i=0;i<MAX_TILES;i++)
	   if (tiles[i].selected == 17)
	   tiles[i].visible = 0 ;*/
                
	timeout_counter = 0;
	timer = g_timeout_add (250, (GSourceFunc) hint_timeout, NULL);
                
	/* 30s penalty */
	games_clock_stop (GAMES_CLOCK(chrono));
	seconds = GAMES_CLOCK(chrono)->stopped;
	games_clock_set_seconds(GAMES_CLOCK(chrono), (gint) (seconds+30));
	games_clock_start (GAMES_CLOCK(chrono));
}

static void
about_destroy (GtkWidget * widget, gpointer data)
{
	about = NULL;
}

void
about_callback (GtkWidget *widget, gpointer data)
{
	GdkPixbuf *pixbuf = NULL;
	const gchar *authors [] = {
		N_("Main game:"),
		"Francisco Bustamante",
		"Max Watson",
		"Heinz Hempe",
		"Michael Meeks",
                "Philippe Chavin",
		"Callum McKenzie",
		"Rexford Newbould",
		"Krzysztof Foltman",
		"",
		N_("Tiles:"),
		"Jonathan Buzzard",
		"Jim Evans",
		"Richard Hoelscher",
		"Gonzalo Odiard",
		"Max Watson",
		NULL
	};
	gchar *documenters[] = {
                NULL
        };
        /* Translator credits */
        gchar *translator_credits = _("translator-credits");

	{
		char *filename = NULL;

		filename = gnome_program_locate_file (NULL,
				GNOME_FILE_DOMAIN_APP_PIXMAP, 
				"gnome-mahjongg.png",
				TRUE, NULL);
		if (filename != NULL) {
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free (filename);
		}
	}

	if (about) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}
	
	about = gnome_about_new (_("GNOME Mahjongg"), VERSION,
				 "Copyright \xc2\xa9 1998-2004 Free Software "
				 "Foundation, Inc.",
				 _("GNOME version of the Eastern tile game, "
				   "Mahjongg."),
				 (const gchar **)authors,
				 (const gchar **)documenters,
				 strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
				pixbuf);

	g_signal_connect (G_OBJECT (about), "destroy",
			  G_CALLBACK (about_destroy), NULL);
	
	if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);
	
	gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (window));
	gtk_widget_show (about);
}

void
pause_callback (void)
{
        paused = !paused;
	draw_all_tiles ();
        if (paused) {
                games_clock_stop (GAMES_CLOCK (chrono));
                message(_("... Game paused ..."));
        }
        else {
                message ("");
                games_clock_start (GAMES_CLOCK(chrono));
        }
}

void ensure_pause_off (void)
{
        if (paused) {
		draw_all_tiles ();
                message("");
        }
        paused = FALSE;
}

void
scores_callback (GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;

	dialog = gnome_scores_display (_(APPNAME_LONG), APPNAME, score_current_mapset, 0);
	if (dialog != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW(dialog),
					      GTK_WINDOW(window));
		gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
	}
}

static void
init_game (void)
{
        gtk_label_set_text (GTK_LABEL (tiles_label), MAX_TILES_STR);
        update_moves_left ();
        game_over = GAME_RUNNING;
        sequence_number = 1 ;
        visible_tiles = MAX_TILES;
        selected_tile = MAX_TILES + 1;
	set_undoredo_sensitive (FALSE, FALSE);
	set_menus_sensitive ();

        chrono_start();
}

void
confirm_action (GtkWidget *widget, gpointer data)
{
	switch ((gint)data) {
	case NEW_GAME:
	case NEW_GAME_WITH_SEED:
		ensure_pause_off ();
		new_game ((gint)data == NEW_GAME);
		break;
	case RESTART_GAME:
		restart_game ();
		break;
	case QUIT_GAME:
		gtk_main_quit ();
		break;
	default:
		break;
	}
}

gboolean
delete_event_callback (GtkWidget *widget, GdkEventAny *any, gpointer data)
{
        confirm_action (widget, data);
        return TRUE;
}

void
restart_game (void)
{
    gint i;

    ensure_pause_off ();
    for (i = 0; i < MAX_TILES; i++) {
        tiles[i].visible = 1;
        tiles[i].selected = 0;
	tiles[i].sequence = 0;
    }
    draw_all_tiles ();
    init_game ();
}

void
redo_tile_callback (GtkWidget *widget, gpointer data)
{
        gint i, change ;
        char *tmpstr;
        
        if (paused) 
                return; 
        if (sequence_number>(MAX_TILES/2))
                return ;
        
        if (selected_tile<MAX_TILES) {
                tiles[selected_tile].selected = 0 ;
                draw_tile(selected_tile);
                selected_tile = MAX_TILES + 1; 
        }
        change = 0 ;
        for (i=0; i<MAX_TILES; i++)
                if (tiles[i].sequence == sequence_number) {
                        tiles[i].selected = 0 ;
                        tiles[i].visible = 0 ;
			draw_tile (i);
                        visible_tiles-- ;
                        change = 1 ;
                }
        if (change) {
                if (sequence_number < MAX_TILES)
                        sequence_number++ ;
        }
        else
                  	gnome_app_flash (GNOME_APP (window), "No more redo!");
        tmpstr = g_strdup_printf ("%3d",visible_tiles);
        gtk_label_set_text(GTK_LABEL (tiles_label), tmpstr);

        set_undoredo_sensitive (TRUE, FALSE);
	for (i=0; i<MAX_TILES; i++) {
		if (tiles[i].sequence == sequence_number)
			set_undoredo_sensitive (TRUE, TRUE);
	}
	
        update_moves_left ();
}

void
undo_tile_callback (GtkWidget *widget, gpointer data)
{
        gint i;
        char *tmpstr;
        
        if (paused || game_over == GAME_WON) 
                return;
        if (game_over == GAME_LOST)
                game_over = GAME_RUNNING;
        if (sequence_number>1)
                sequence_number-- ;
        else
                return ;
        
        if (selected_tile<MAX_TILES) {
                tiles[selected_tile].selected = 0 ;
                draw_tile (selected_tile);
                selected_tile = MAX_TILES + 1; 
        }
        
        for (i=0; i<MAX_TILES; i++)
                if (tiles[i].sequence == sequence_number) {
                        tiles[i].selected = 0 ;
                        tiles[i].visible = 1 ;
                        visible_tiles++ ;
			draw_tile (i);
                }

        tmpstr = g_strdup_printf ("%3d", visible_tiles);
        gtk_label_set_text (GTK_LABEL(tiles_label), tmpstr);
	g_free (tmpstr);

	set_menus_sensitive ();
	set_undoredo_sensitive (sequence_number>1, TRUE);
        update_moves_left ();
}

void
select_game (GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog, *entry, *label;
	GtkWidget *box;
	gint response;

	dialog = gtk_dialog_new_with_buttons (_("Select Game"),
						 GTK_WINDOW (window),
						 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_STOCK_CANCEL,
						 GTK_RESPONSE_CANCEL,
						 GTK_STOCK_OK,
						 GTK_RESPONSE_OK,
						 NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	box = GTK_DIALOG (dialog)->vbox;

	gtk_box_set_spacing (GTK_BOX (box), 8);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 8);

	label = gtk_label_new (_("Game Number:"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start_defaults (GTK_BOX(box), label);
	
	entry = gtk_entry_new ();
	gtk_box_pack_start_defaults (GTK_BOX(box), entry);
	
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_window_set_focus (GTK_WINDOW (dialog), entry);

	gtk_widget_show_all (dialog);
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	
	if (response == GTK_RESPONSE_OK) {
		next_seed = atoi (gtk_entry_get_text (GTK_ENTRY (entry)));
		gtk_widget_destroy (dialog);
		confirm_action (widget, (gpointer) NEW_GAME_WITH_SEED);
	} else {
              	gtk_widget_destroy (dialog);
	}
}

void
sound_on_callback (GtkWidget *widget, gpointer data)
{
	printf ("mer\n");
}

/* You loose your re-do queue when you make a move */
static void
clear_undo_queue (void)
{
	gint lp ;

	for (lp=0;lp<MAX_TILES;lp++)
		if (tiles[lp].sequence>=sequence_number)
			tiles[lp].sequence = 0 ;
}

static void
load_map (void)
{
	gchar* name = mapset;
	gint lp ;
	gboolean found;

	new_map = FALSE;
		
	found = FALSE;
	for (lp=0;lp<G_N_ELEMENTS(maps);lp++)
		if (g_ascii_strcasecmp (maps[lp].name, name) == 0) {
			found = TRUE;
			break;
		}

	if (!found) {
		lp = 0;
		g_free (mapset);
		mapset = g_strdup (maps[0].name);
		/* We don't set the gconf key to avoid warning messages appearing multiple times.
		 * Yes, I know this is a bad excuse. */
	}

	pos = maps[lp].map ;

	generate_dependencies ();
	calculate_view_geometry ();
	configure_pixmaps (); 
}

static void
do_game (void)
{
	char *str;

	current_seed = next_seed;
	str = g_strdup_printf ("%s (%d)",_("Mahjongg"), current_seed);
	gtk_window_set_title (GTK_WINDOW (window), str);
	g_free (str);

	if (new_map)
		load_map ();
	generate_game (current_seed); /* puts in the positions of the tiles */
}

/* Record any changes to our window size. */
static gboolean window_configure_cb (GtkWidget *w, GdkEventConfigure *e,
				 gpointer data)
{
	gconf_client_set_int (conf_client, "/apps/mahjongg/width",
			      e->width, NULL);
	gconf_client_set_int (conf_client, "/apps/mahjongg/height",
			      e->height, NULL);

	return FALSE;
}

static void
load_preferences (void)
{
	gchar *buf;
	
	mapset = gconf_client_get_string (conf_client,
			"/apps/mahjongg/mapset", NULL);

	buf = gconf_client_get_string (conf_client,
			"/apps/mahjongg/bgcolour", NULL) ;
	set_background (buf) ;
	g_free (buf);

	buf = gconf_client_get_string (conf_client, "/apps/mahjongg/tileset", NULL);
	if (buf == NULL) {
		buf = g_strdup("default.png");
	}
	selected_tileset = g_strdup (buf);
	g_free (buf);	
	
	popup_warn = gconf_client_get_bool (conf_client,
					    "/apps/mahjongg/warn", NULL);

	windowwidth = gconf_client_get_int (conf_client,
					    "/apps/mahjongg/width", NULL);
	if (windowwidth <= 0)
		windowwidth = 530;
	
	windowheight = gconf_client_get_int (conf_client,
					     "/apps/mahjongg/height", NULL);
	if (windowheight <= 0)
		windowheight = 440;

	
	load_images (selected_tileset);
}

static void
new_seed ()
{
	struct timeval t;
	gettimeofday (&t, NULL);

	next_seed = (guint) (t.tv_sec ^ t.tv_usec);
}

static void set_score_file (gchar * mapset)
{
	int i;

	/* FIXME: This is a bit ugly, but we only save the name of the
	   map (and it isn't suitable for generating the scorefile
	   name. It's also a bit close to code freeze to introduce
	   gratuitous changes so this is it. */
	for (i=0; i<G_N_ELEMENTS(maps); i++) {
		if (g_utf8_collate (mapset, maps[i].name) == 0)
			score_current_mapset = maps[i].score_name;
	}
}

void
new_game (gboolean re_seed)
{
	if (re_seed)
		new_seed ();

	do_game ();

	draw_all_tiles ();

	init_game ();

	set_score_file (mapset);

	update_score_state ();
}

void
shuffle_tiles_callback (GtkWidget *widget, gpointer data)
{
        time_t seconds;
	gboolean ok;

        if (paused || game_over == GAME_DEAD || game_over == GAME_WON) return;

	/* Make sure no tiles are selected. */
	if (selected_tile < MAX_TILES) {
		unselect_tile(selected_tile);
	}

	ok = shuffle ();
   
	if (!ok) {
                GtkWidget *mb;
                game_over = GAME_DEAD;
                games_clock_stop (GAMES_CLOCK (chrono));
		mb = gtk_message_dialog_new (GTK_WINDOW (window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_OK,
					     (_("Sorry, I was unable to find a playable configuration.")));
		gtk_dialog_run (GTK_DIALOG (mb));
		gtk_widget_destroy (mb);

        } else {

                draw_all_tiles ();
                
                game_over = GAME_RUNNING;

                /* 60s penalty */
                games_clock_stop (GAMES_CLOCK(chrono));
                seconds = GAMES_CLOCK(chrono)->stopped;
                games_clock_set_seconds(GAMES_CLOCK(chrono), (gint) (seconds+60));
                games_clock_start (GAMES_CLOCK(chrono));
		
		update_moves_left ();

		/* Disable undo/redo after a shuffle. */
		sequence_number = 1;
		clear_undo_queue ();
		set_undoredo_sensitive (FALSE, FALSE);
        }
	
	set_menus_sensitive ();
}

int
main (int argc, char *argv [])
{
	GtkWidget *board;
	GtkWidget *chrono_label;
	GtkWidget *status_box;
	GtkWidget *group_box;
	GtkWidget *spacer;
	gboolean show=TRUE;

	gnome_score_init (APPNAME);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init (APPNAME, VERSION,
			    LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_APP_DATADIR, DATADIR,
			    NULL);

	gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gnome-mahjongg.png");

	conf_client = gconf_client_get_default ();
	if (!games_gconf_sanity_check_string (conf_client, "/apps/mahjongg/tileset")) {
		return 1;
	}
	load_preferences ();
	
	new_seed ();

	window = gnome_app_new (APPNAME, _(APPNAME_LONG));
	gtk_window_set_default_size (GTK_WINDOW (window), windowwidth,
				     windowheight);
	g_signal_connect (G_OBJECT (window), "configure_event",
			  G_CALLBACK (window_configure_cb), NULL);
	
	/* Statusbar for a chrono, Tiles left and Moves left */
	status_box = gtk_hbox_new (FALSE, 10);

	group_box = gtk_hbox_new (FALSE, 0);
	tiles_label = gtk_label_new (_("Tiles Left:"));
	gtk_box_pack_start (GTK_BOX (group_box), tiles_label, FALSE, FALSE, 0);
	spacer = gtk_label_new (" ");
	gtk_box_pack_start (GTK_BOX (group_box), spacer, FALSE, FALSE, 0);
	tiles_label = gtk_label_new (MAX_TILES_STR);
	gtk_box_pack_start (GTK_BOX (group_box), tiles_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (status_box), group_box, FALSE, FALSE, 0);

	group_box = gtk_hbox_new (FALSE, 0);
	moves_label = gtk_label_new(_("Moves Left:"));
	gtk_box_pack_start (GTK_BOX (group_box), moves_label, FALSE, FALSE, 0);
	spacer = gtk_label_new (" ");
	gtk_box_pack_start (GTK_BOX (group_box), spacer, FALSE, FALSE, 0);
	moves_label = gtk_label_new (MAX_TILES_STR);
	gtk_box_pack_start (GTK_BOX (group_box), moves_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (status_box), group_box, FALSE, FALSE, 0);

	group_box = gtk_hbox_new (FALSE, 0);
	chrono_label = gtk_label_new (_("Time:"));
	gtk_box_pack_start (GTK_BOX (group_box), chrono_label, FALSE, FALSE, 0);
	spacer = gtk_label_new (" ");
	gtk_box_pack_start (GTK_BOX (group_box), spacer, FALSE, FALSE, 0);
	chrono = games_clock_new ();
	gtk_box_pack_start (GTK_BOX (group_box), chrono, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (status_box), group_box, FALSE, FALSE, 0);

	/* show the status bar items */
	gtk_widget_show_all (status_box);

	appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_USER);
	gtk_box_pack_end(GTK_BOX(appbar), status_box, FALSE, FALSE, 0);
	gnome_app_set_statusbar (GNOME_APP (window), appbar);

	gnome_app_create_menus (GNOME_APP (window), mainmenu);
	gnome_app_install_menu_hints(GNOME_APP (window), mainmenu);

	gnome_app_create_toolbar (GNOME_APP (window), toolbar_uiinfo);

	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (delete_event_callback), (gpointer)QUIT_GAME);

	board = create_mahjongg_board ();
	gnome_app_set_contents (GNOME_APP (window), board);

	/* FIXME: get these in the best place (as per the comment below. */
	init_config ();

	do_game ();
	init_game ();
	set_score_file (mapset);
	update_score_state ();
	
	/* Note: we have to have a layout loaded before here so that the
	 * window knows how big to make the tiles. */
	gtk_widget_show (window);

	show = gconf_client_get_bool (conf_client,
			"/apps/mahjongg/show_toolbar", NULL);

	if (show) {
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(settingsmenu[0].widget), TRUE);
	} else {
		BonoboDockItem *gdi;
		
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(settingsmenu[0].widget), FALSE);
		gdi = gnome_app_get_dock_item_by_name
			(GNOME_APP (window), GNOME_APP_TOOLBAR_NAME);
		gtk_widget_hide(GTK_WIDGET(gdi)) ;
		gtk_widget_queue_resize (window);
	}

  	gnome_app_flash (GNOME_APP (window), 
  				_("Welcome to GNOME Mahjongg!")); 

	gtk_main ();
	
	return 0;
}
