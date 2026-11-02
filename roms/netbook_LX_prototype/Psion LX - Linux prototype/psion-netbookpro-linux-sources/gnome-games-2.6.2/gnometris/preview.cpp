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

#include "preview.h"
#include "blocks.h"

Preview::	Preview()
{
	w = gtk_drawing_area_new();

	g_signal_connect (w, "expose_event", G_CALLBACK (expose), this);
	g_signal_connect (w, "configure_event", G_CALLBACK (configure), this);

	gtk_widget_show (w);
}

void
Preview::updateSize()
{
	gtk_widget_set_size_request (w, PREVIEW_SIZE * BLOCK_SIZE, 
				     PREVIEW_SIZE * BLOCK_SIZE);
}

gint
Preview::configure(GtkWidget * widget, GdkEventConfigure * event, Preview * preview)
{
	preview->width = event->width;
	preview->height = event->height;

	return TRUE;
}

gint
Preview::expose(GtkWidget * widget, GdkEventExpose * event, Preview * preview)
{
	GdkRectangle *area = &(event->area);

	gdk_draw_rectangle (widget->window, widget->style->black_gc, TRUE,
			    area->x, area->y, area->width, area->height);

	int xoffs = (preview->width 
		     - sizeTable[blocknr_next][rot_next][1] * BLOCK_SIZE) / 2;
	int yoffs = (preview->height 
		     - sizeTable[blocknr_next][rot_next][0] * BLOCK_SIZE) / 2;

	xoffs -= offsetTable[blocknr_next][rot_next][1] * BLOCK_SIZE;
	yoffs -= offsetTable[blocknr_next][rot_next][0] * BLOCK_SIZE;

	if (do_preview)
	{
		if (blocknr_next == -1)
			return TRUE;

		for (int x = 0; x < 4; ++x) {
			for (int y = 0; y < 4; ++y) {
				if (blockTable[blocknr_next][rot_next][x][y])	
					gdk_draw_pixbuf (widget->window, widget->style->black_gc, pic[color_next],
							 0, 0, 
							 x * BLOCK_SIZE + xoffs, y * BLOCK_SIZE + yoffs,
							 BLOCK_SIZE, BLOCK_SIZE,
							 GDK_RGB_DITHER_NORMAL, 0, 0);
				
			}
		}
	}
	else
	{
		gdk_draw_line(widget->window, widget->style->white_gc, 0, 0, 
			      preview->width, preview->height);
		gdk_draw_line(widget->window, widget->style->white_gc, 0, 
			      preview->height, preview->width, 0);
	}

	return TRUE;
}
