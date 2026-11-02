/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


/* Some defines for the outlook etc. */
#ifndef DEFINES_H
#define DEFINES_H


// ----------------------------------------------------------------
//                        Graphical layout


#define cardHeight         96 
#define cardWidth          72

#define cardDistFromTop    20
#define cardDistFromBottom 10
#define cardHDist          10

#define CLHBorderDistance   5
#define CLHDistFromTop    180
#define CLHWidth          140

// Some derived constants
#define PLAYERBOX_WIDTH   ((cardWidth + cardHDist) * PokerHandSize + cardHDist + 135)
#define PLAYERBOX_HEIGHT  (cardHeight + cardDistFromTop + cardDistFromBottom)
#define DISTANCE_FROM_2ND_BOX 100

#define wonLabelVDist (cardHeight + cardDistFromTop + cardDistFromBottom + 10)
#define clickToHoldVDist 130

#define BORDER                        10
#define PLAYERBOX_BORDERS             15
#define PLAYERBOX_HDISTANCEOFWIDGETS  10


#define MAX_PLAYERS 2 // TODO: maximal 4-> see lskat for computer players


// ----------------------------------------------------------------
//                           Default values


#define START_MONEY      100
#define DEFAULT_PLAYERS  1 // change to 2 later

// Will be overridden by config file:
#define SOUND_DEFAULT        true
#define BLINKING_DEFAULT     true
#define SHOWNEWGAME_DEFAULT  false
#define ADJUST_DEFAULT       true
#define LOADGAME_DEFAULT     true

// changeable in OptionsDlg (and will be overridden by config file):
#define DRAWDELAY      300 
#define MIN_BET          5
#define MAX_BET         20
#define CASH_PER_ROUND   5


#endif
