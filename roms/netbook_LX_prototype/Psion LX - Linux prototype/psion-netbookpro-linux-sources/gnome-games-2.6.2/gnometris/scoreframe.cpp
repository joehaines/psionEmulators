/* -*- mode:C++; tab-width:8; c-basic-offset:8; indent-tabs-mode:true -*- */

/*
 * written by J. Marcin Gorycki <marcin.gorycki@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * For more details see the file COPYING.
 */

#include <config.h>
#include <gnome.h>
#include <games-gconf.h>
#include <games-frame.h>

#include "scoreframe.h"

// FIXME: This is also defined in tetris.cpp
#define KEY_STARTING_LEVEL "/apps/gnometris/options/starting_level"

ScoreFrame::ScoreFrame(int cmdlLevel)
	: score(0), lines(0)
{
	if (cmdlLevel) 
		startingLevel = cmdlLevel;
	else {
		GConfClient *gconf_client = gconf_client_get_default ();
		
		startingLevel = gconf_client_get_int (gconf_client,
						      KEY_STARTING_LEVEL,
						      NULL);
		if (startingLevel < 1)
			startingLevel = 1;
		if (startingLevel > 10)
			startingLevel = 10;
		g_object_unref (gconf_client);
	}
	if (startingLevel < 1)
		startingLevel = 1;

	level = startingLevel;
	
	w = gtk_table_new (3, 2, FALSE);

	scoreLabel = gtk_label_new (_("Score:"));
	gtk_misc_set_alignment (GTK_MISC (scoreLabel), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (w), scoreLabel,
				   0, 1, 0, 1);
	sprintf(b, "%7d", 0);
	scorew = gtk_label_new (b);
	gtk_misc_set_alignment (GTK_MISC (scorew), 1.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (w), scorew,
				   1, 2, 0, 1);

	linesLabel = gtk_label_new (_("Lines:"));
	gtk_misc_set_alignment (GTK_MISC (linesLabel), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (w), linesLabel,
				   0, 1, 1, 2);
	sprintf(b, "%7d", 0);
	linesw = gtk_label_new (b);
	gtk_misc_set_alignment (GTK_MISC (linesw), 1.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (w), linesw,
				   1, 2, 1, 2);


	levelLabel = gtk_label_new (_("Level:"));
	gtk_misc_set_alignment (GTK_MISC (levelLabel), 0.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (w), levelLabel,
				   0, 1, 2, 3);
	sprintf(b, "%7d", level);
	levelw = gtk_label_new (b);
	gtk_misc_set_alignment (GTK_MISC (levelw), 1.0, 0.5);
	gtk_table_attach_defaults (GTK_TABLE (w), levelw,
				   1, 2, 2, 3);

}

void
ScoreFrame::show()
{
	gtk_widget_show_all (w);
}

void 
ScoreFrame::setScore(int s)
{
	score = s;
	
	sprintf(b, "%7d", score);
	gtk_label_set_text(GTK_LABEL(scorew), b);
}

void
ScoreFrame::incScore(int s)
{
	score += s;
	
	sprintf(b, "%7d", score);
	gtk_label_set_text(GTK_LABEL(scorew), b);
}

void
ScoreFrame::resetLines()
{
	lines = 0;
	score = 0;
	sprintf(b, "%7d", lines);
	gtk_label_set_text(GTK_LABEL(scorew), b);
	gtk_label_set_text(GTK_LABEL(linesw), b);
}

void 
ScoreFrame::incLines(int newlines)
{
	int linescore = 0;

	lines += newlines;
	switch(newlines)
	{
		case 1:
			linescore = 40;
			break;
		case 2:
			linescore = 100;
			break;
		case 3:
			linescore = 300;
			break;
		case 4:
			linescore = 1200;
			break;
	}
	incScore(linescore * level);
	sprintf(b, "%7d", lines);
	gtk_label_set_text(GTK_LABEL(linesw), b);
}

void 
ScoreFrame::checkLevel()
{
	int l = startingLevel + lines / 10;
	if ((l > level) && (l <= 10))
	{
		level = l;
		sprintf(b, "%7d", level);
		gtk_label_set_text(GTK_LABEL(levelw), b);
	}
}

void 
ScoreFrame::setLevel(int l)
{
	level = l;
	sprintf(b, "%7d", level);
	gtk_label_set_text(GTK_LABEL(levelw), b);
}

void
ScoreFrame::setStartingLevel(int l)
{
	startingLevel = l;
}




