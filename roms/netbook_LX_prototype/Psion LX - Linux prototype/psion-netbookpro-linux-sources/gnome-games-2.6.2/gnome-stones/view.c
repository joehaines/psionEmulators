/* -*- mode:C; indent-tabs-mode:t; tab-width:8; c-basic-offset:2; -*- */

/* gnome-stones - main.c
 *
 * Time-stamp: <1999/01/18 18:50:24 carsten>
 *
 * Copyright (C) 1998, 1999, 2003 Carsten Schaar
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "main.h"
#include "view.h"
#include "object.h"
#include "types.h"
#include "preferences.h"

/*****************************************************************************/
/* Some declarations */


gint view_expose_event_cb (GtkWidget *widget, GdkEventExpose *event, 
			   gpointer data);
static gint view_expose (GtkWidget *widget, GdkEventExpose *event);

extern GStonesCave *cave;


/*****************************************************************************/
/* Stuff that is needed for the GTK* type system */


static void
view_class_init (GStonesViewClass *class)
{
  GtkWidgetClass *widget_class;
 
  widget_class= (GtkWidgetClass *) class;



#ifndef USE_GNOME_CANVAS
  widget_class->expose_event= view_expose;
#endif
}



static void
view_init (GStonesView *view)
{
  view->display_mode        = DISPLAY_IMAGE;

  /* cave display */
  view->x_offset            = 0;
  view->y_offset            = 0;
  view->x_scrolling         = TRUE;
  view->y_scrolling         = TRUE;

  /* image display */
  view->image               = NULL;
  
  /* curtain display */
  view->curtain_display_mode= CURTAIN_DISPLAY_NONE;
  view->curtain_timeout     = 0;
  view->curtain             = 0;
}


GType
view_get_type (void)
{
  static GType view_type= 0;

  if (!view_type)
    {
      GTypeInfo view_info=
      {
	sizeof (GStonesViewClass),

        NULL,
        NULL,

	(GClassInitFunc) view_class_init,
        NULL,
        NULL,

	sizeof (GStonesView),
        0,
	(GInstanceInitFunc) view_init,
        
	NULL
      };
      
      view_type= g_type_register_static (gtk_drawing_area_get_type (), 
                                         "GStonesView", 
                                         &view_info,
                                         0 );
    }
  
  return view_type;
} 
  

/*****************************************************************************/


GtkWidget *
view_new (GdkPixbuf *curtain_image)
{
  GStonesView    *view;
#ifdef USE_GNOME_CANVAS
  GnomeCanvas    *canvas;
#else
  GtkDrawingArea *drawing_area;
#endif
  GtkWidget      *widget;
  GtkObject      *object;

  g_return_val_if_fail (curtain_image, NULL);

  gtk_widget_push_colormap (gdk_rgb_get_colormap ());

  view= g_object_new (view_get_type (), NULL);
  
  gtk_widget_pop_colormap ();

#ifdef USE_GNOME_CANVAS
  canvas      = GNOME_CANVAS (view);
#else
  drawing_area= GTK_DRAWING_AREA (view);
#endif
  widget      = GTK_WIDGET (view);
  object      = GTK_OBJECT (view);

#ifndef USE_GNOME_CANVAS

  gtk_widget_set_events (widget, gtk_widget_get_events (widget) | GAME_EVENTS);
  
  gtk_widget_set_size_request (GTK_WIDGET (drawing_area), 
                               GAME_COLS * STONE_SIZE,
                               GAME_ROWS * STONE_SIZE);

  gtk_widget_show (widget);

#else

  gtk_widget_push_colormap (gdk_rgb_get_colormap ());

  view->canvas= gtk_drawing_area_new ();
  canvas= gnome_canvas_new ();

  gtk_widget_pop_colormap ();
  
  gtk_widget_set_events (view->canvas, gtk_widget_get_events (view->canvas) | GAME_EVENTS);

  gtk_drawing_area_size (GTK_DRAWING_AREA (view->canvas),
			 GAME_COLS * STONE_SIZE,
			 GAME_ROWS * STONE_SIZE);

  /* Now for some experimantal gnome canvas stuff.  */
  gtk_widget_set_usize (canvas, GAME_COLS*STONE_SIZE, GAME_ROWS*STONE_SIZE);
  gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas),
				  0, 0,
				  GAME_COLS*STONE_SIZE,
				  GAME_ROWS*STONE_SIZE);

  gtk_widget_show (canvas);

  gtk_widget_show (view->canvas);
#endif


  view->view_buffer = NULL;

  /* Initialize curtain stuff.  */
  gdk_pixbuf_render_pixmap_and_mask (curtain_image, &view->curtain_image, NULL, 127);
  
  view->curtain_display_mode= CURTAIN_DISPLAY_CLOSING;
  view->curtain             = 0;

  return GTK_WIDGET (view);
}



/*****************************************************************************/

/* we want some cute cutain :-)*/

#define CURTAIN_START_VALUE 31

static gboolean
curtain_visible_test (int x, int y, int curtain)
{
  int a = (2 * x * (x + y) + 3 * y * GAME_COLS);
  return ((a*a) % CURTAIN_START_VALUE)
    * CURTAIN_START_VALUE * CURTAIN_START_VALUE
    >= curtain * curtain * curtain;
}


/*****************************************************************************/



static gint
view_expose (GtkWidget *widget, GdkEventExpose *event)
{
  GStonesView  *view;
  GdkRectangle *area;
  int x1, y1, x2, y2, x, y;
  int curtain_frames = 1;
  if( cave && cave->animated_curtain )
    {
      curtain_frames=cave->animated_curtain->num_images;
    }


  view= GSTONES_VIEW (widget);

  /* Initialize image buffer */

  if (!view->view_buffer)
    view->view_buffer = 
      gdk_pixmap_new (widget->window, CAVE_MAX_WIDTH*STONE_SIZE,
		                      CAVE_MAX_HEIGHT*STONE_SIZE, -1 );
  area= &event->area;

  x1 = (area->x+view->x_offset)/STONE_SIZE;
  y1 = (area->y+view->y_offset)/STONE_SIZE;
  x2 = (area->x+area->width+view->x_offset+STONE_SIZE-1)/STONE_SIZE;
  y2 = (area->y+area->height+view->y_offset+STONE_SIZE-1)/STONE_SIZE;
  
  for (x = x1; x <= x2; x++)
    for (y = y1; y <= y2; y++)
      {
	GdkPixmap *image= NULL;
	
	if ((view->curtain_display_mode == CURTAIN_DISPLAY_CLOSING && 
	     curtain_visible_test (x, y, view->curtain)) ||
	    (view->curtain_display_mode == CURTAIN_DISPLAY_OPENING && 
	     !curtain_visible_test (x, y, view->curtain)))
	  {
	    if( cave && cave->animated_curtain )
	      {
	      image = 
		object_get_image (cave->animated_curtain, 
                      /*curtain_frames-1-*/(view->curtain)%curtain_frames);
	      }
	    else
	      image= view->curtain_image;
	  }
	else if (view->display_mode == DISPLAY_IMAGE)
	  {
	    image= NULL;
	  }
	else if (cave)
	  {
	    image= cave_get_image (cave, x+1, y+1);
	  }

	if (image)
	  {
	    if (image!=view->last_image[x][y])
	      {
		view->last_image[x][y]=image;
		gdk_draw_drawable (view->view_buffer,
                                   widget->style->black_gc, image,
                                   0, 0, 
                                   x*STONE_SIZE, 
                                   y*STONE_SIZE,
                                   STONE_SIZE, STONE_SIZE);
	      }
	  }
	else
	  {
	    
	    view->last_image[x][y]= view->image;
	    gdk_draw_drawable (view->view_buffer,
                               widget->style->black_gc, view->image,
                               x*STONE_SIZE-view->x_offset, 
                               y*STONE_SIZE-view->y_offset, 
                               x*STONE_SIZE, 
                               y*STONE_SIZE,
                               STONE_SIZE, STONE_SIZE);
	  }
 
     }

  gdk_draw_drawable (widget->window,
                     widget->style->black_gc, view->view_buffer,
                     area->x+view->x_offset, 
                     area->y+view->y_offset, 
                     area->x, 
                     area->y,
                     area->width, area->height);

  return TRUE;
}



/*****************************************************************************/

static gint
view_curtain_timeout (gpointer data)
{
  GStonesView     *view= (GStonesView *) data;

  if (view->curtain_display_mode == CURTAIN_DISPLAY_CLOSING)
    {
      if (view->curtain > 0)
	{
	  /* The curtain is not closed yet.  */
	  
	  view->curtain--;
	  gtk_widget_queue_draw_area (GTK_WIDGET (view), 0, 0,
				      GAME_COLS * STONE_SIZE,
				      GAME_ROWS * STONE_SIZE);
	  return TRUE;
	}

      if (view->curtain_func)
	view->curtain_func (VIEW_CURTAIN_CLOSED);
      
      view->curtain_display_mode= CURTAIN_DISPLAY_OPENING;
      view->curtain             = GAME_COLS+GAME_ROWS;
      
      return TRUE;
    }
  else
    {
      if (view->curtain > 0)
	{
	  /* The curtain is not closed yet.  */
	  
	  view->curtain--;
	  gtk_widget_queue_draw_area (GTK_WIDGET (view), 0, 0,
				      GAME_COLS * STONE_SIZE,
				      GAME_ROWS * STONE_SIZE);
	  return TRUE;
	}
      
      view->curtain_timeout= 0;

      if (view->curtain_func)
	view->curtain_func (VIEW_CURTAIN_OPEN);
      
      return FALSE;
    }
}


void
view_set_curtain_mode (GStonesView     *view,
		       ViewCurtainMode  mode, 
		       ViewCurtainFunc  func)
{
  g_return_if_fail (view);

  if (view->curtain_timeout)
    {
      g_source_remove (view->curtain_timeout);
      view->curtain_timeout= 0;
    }
  
  switch (mode)
    {
    case VIEW_CURTAIN_OPEN:
      view->curtain_display_mode= CURTAIN_DISPLAY_NONE;
      gtk_widget_queue_draw_area (GTK_WIDGET (view), 0, 0,
                                  GAME_COLS * STONE_SIZE,
                                  GAME_ROWS * STONE_SIZE);
      break;
      
    case VIEW_CURTAIN_ANIMATE:
      view->curtain_display_mode= CURTAIN_DISPLAY_CLOSING;
      view->curtain             = CURTAIN_START_VALUE;
      view->curtain_func        = func;


      view->curtain_timeout= 
	g_timeout_add (CURTAIN_DELAY, view_curtain_timeout, (gpointer) view);
      break;

    case VIEW_CURTAIN_CLOSED:
      view->curtain_display_mode= CURTAIN_DISPLAY_CLOSING;
      view->curtain             = 0;
      gtk_widget_queue_draw_area (GTK_WIDGET (view), 0, 0,
                                  GAME_COLS * STONE_SIZE,
                                  GAME_ROWS * STONE_SIZE);
      break;      
    }
}

void
view_display_image (GStonesView *view, GdkPixmap *image)
{
  g_return_if_fail (view);
  g_return_if_fail (GSTONES_IS_VIEW (view));
  
  if (image)
    {
      if (view->image)
	g_object_unref (view->image);
      
      view->image= image;
      g_object_ref (view->image);
    }

  view->display_mode= DISPLAY_IMAGE;
  gtk_widget_queue_draw_area (GTK_WIDGET (view), 0, 0,
                              GAME_COLS * STONE_SIZE,
                              GAME_ROWS * STONE_SIZE);
}


void
view_display_cave (GStonesView *view, GStonesCave *cave)
{
  g_return_if_fail (view);
  g_return_if_fail (cave);
  g_return_if_fail (GSTONES_IS_VIEW (view));
  
  view->display_mode= DISPLAY_CAVE;
  gtk_widget_queue_draw_area (GTK_WIDGET (view), 0, 0,
                              GAME_COLS * STONE_SIZE,
                              GAME_ROWS * STONE_SIZE);
}


void
atari_scroll (GStonesView *view, GStonesCave *cave)
{
  gint x_rel;
  gint y_rel;
  
  x_rel= cave->player_x-view->x_offset/STONE_SIZE;
  y_rel= cave->player_y-view->y_offset/STONE_SIZE;
  
  view->x_scrolling= view->x_scrolling || (x_rel < 3) || (x_rel > GAME_COLS+1-3) || 
    (view->x_offset+GAME_COLS*STONE_SIZE > cave->width*STONE_SIZE);
  view->y_scrolling= view->y_scrolling || (y_rel < 3) || (y_rel > GAME_ROWS+1-3) || 
    (view->y_offset+GAME_ROWS*STONE_SIZE > cave->height*STONE_SIZE);
  
  if (view->x_scrolling)
    {
      if (((x_rel < 7) || (view->x_offset+GAME_COLS*STONE_SIZE > cave->width*STONE_SIZE)))
	{
	  view->x_offset-=SCROLL_SPEED;
	  if( view->x_offset<0 ) view->x_offset=0;
	}
      else if ((x_rel > GAME_COLS+1-7))
	{
	  view->x_offset+=SCROLL_SPEED;

	  if( view->x_offset>(cave->width-GAME_COLS)*STONE_SIZE ) 
	    view->x_offset=(cave->width-GAME_COLS)*STONE_SIZE;
	}
      else
	view->x_scrolling= FALSE;
    }

  if (view->y_scrolling)
    {      
      if (((y_rel < 5) || (view->y_offset+GAME_ROWS*STONE_SIZE > cave->height*STONE_SIZE)))
	{
	  view->y_offset-=SCROLL_SPEED;
	  if( view->y_offset<0 ) view->y_offset=0;
	}
      else if ((y_rel > GAME_ROWS+1-5))
	{
	  view->y_offset+=SCROLL_SPEED;

	  if( view->y_offset>(cave->height-GAME_ROWS)*STONE_SIZE ) 
	    view->y_offset=(cave->height-GAME_ROWS)*STONE_SIZE;

	}
      else
	view->y_scrolling= FALSE;
    }
}









#define MAX_SCROLL_SPEED 0.9

#ifndef abs
#define abs(i) ((i)>0?(i):-(i))
#endif

void
smooth_scroll (GStonesView *view, GStonesCave *cave)
{
  gint x_rel;
  gint y_rel;
  gint x_mov;
  gint y_mov;
  gint x_mid=GAME_COLS*STONE_SIZE/2;
  gint y_mid=GAME_ROWS*STONE_SIZE/2;

  x_rel= (STONE_SIZE*cave->player_x-view->x_offset)-STONE_SIZE/2;
  x_mov= (x_rel-x_mid)*2/GAME_COLS;
  x_mov+=(x_rel>x_mid)?1:-1;

  y_rel= (STONE_SIZE*cave->player_y-view->y_offset)-STONE_SIZE/2;
  y_mov= (y_rel-y_mid)*2/GAME_ROWS;
  y_mov+=(y_rel>y_mid)?1:-1;

  
  view->x_scrolling= view->x_scrolling || abs(x_rel-x_mid) > 1;
  view->y_scrolling= view->y_scrolling || abs(y_rel-y_mid) > 1;
  

  if (view->x_scrolling)
    {

      if (x_mov<-MAX_SCROLL_SPEED*STONE_SIZE) 
        x_mov=-MAX_SCROLL_SPEED*STONE_SIZE;
      if (x_mov>MAX_SCROLL_SPEED*STONE_SIZE) 
        x_mov=MAX_SCROLL_SPEED*STONE_SIZE;
 
      view->x_offset+=x_mov;
      if (abs(x_rel-x_mid)<2) view->x_scrolling=FALSE;

      
      if( x_mov>0 && view->x_offset>(cave->width-GAME_COLS)*STONE_SIZE ) 
	view->x_offset=(cave->width-GAME_COLS)*STONE_SIZE;

      if( view->x_offset<0 ) view->x_offset=0;

    }

  if (view->y_scrolling)
    {
      if (y_mov<-MAX_SCROLL_SPEED*STONE_SIZE) 
        y_mov=-MAX_SCROLL_SPEED*STONE_SIZE;
      if (y_mov>MAX_SCROLL_SPEED*STONE_SIZE) 
        y_mov=MAX_SCROLL_SPEED*STONE_SIZE;
 
      view->y_offset+=y_mov;
      if (abs(y_rel-y_mid)<2) view->y_scrolling=FALSE;

      if( y_mov>0 && view->y_offset>(cave->height-GAME_ROWS)*STONE_SIZE ) 
	view->y_offset=(cave->height-GAME_ROWS)*STONE_SIZE;

      if( view->y_offset<0 ) view->y_offset=0;
      
    }



}


void
center_scroll (GStonesView *view, GStonesCave *cave)
{

  view->x_offset= 
     STONE_SIZE*cave->player_x-STONE_SIZE/2-GAME_COLS*STONE_SIZE/2;

  view->y_offset= 
     STONE_SIZE*cave->player_y-STONE_SIZE/2-GAME_ROWS*STONE_SIZE/2;



  if( view->x_offset>(cave->width-GAME_COLS)*STONE_SIZE ) 
    view->x_offset=(cave->width-GAME_COLS)*STONE_SIZE;

  if( view->x_offset<0 ) view->x_offset=0;
      



  if( view->y_offset>(cave->height-GAME_ROWS)*STONE_SIZE ) 
    view->y_offset=(cave->height-GAME_ROWS)*STONE_SIZE;

  if( view->y_offset<0 ) view->y_offset=0;
      


}




void
view_calculate_offset (GStonesView *view, GStonesCave *cave)
{
  view_scroll_method (view, cave); 
}
