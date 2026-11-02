/* -*- mode:C++; tab-width:8; c-basic-offset:8; indent-tabs-mode:true -*- */

/*
 * This is a temporary hack until somebody optimize GnomeCanvasPixbuf.
 * I just get the GnomeCanvasImage that comes with gnome-libs and ported
 * it to pixbuf and remove some stuff.
 *
 * I don't know if this bits are corrects, i don't know much about images,
 * i just try to get the simple thing that work.
 *
 * If you make changes on this file please also update:
 *
 * gnome-games/mahjongg
 * gnome-games/gnometris
 *
 * Copyright (C) 1998, 2003 The Free Software Foundation
 * Copyright (C) 2001 Juan Pablo Mendoza Mendoza
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx> (Original code).
 * 	    Juan Pablo Mendoza <pablo_juan@yahoo.com>
 */

#undef G_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED

#include <config.h>
#include <math.h>
#include "gnome-canvas-pimage.h"
#include "libgnomecanvas/gnome-canvas-util.h"
#include "libgnomeui/gnometypebuiltins.h"


enum {
	ARG_0,
	ARG_IMAGE,
	ARG_X,
	ARG_Y,
	ARG_WIDTH,
	ARG_HEIGHT,
};


static void gnome_canvas_pimage_class_init (GnomeCanvasPImageClass *class);
static void gnome_canvas_pimage_init       (GnomeCanvasPImage *image);
static void gnome_canvas_pimage_destroy    (GtkObject *object);
static void gnome_canvas_pimage_set_arg    (GtkObject             *object,
					   GtkArg                *arg,
					   guint                  arg_id);
static void gnome_canvas_pimage_get_arg    (GtkObject             *object,
					   GtkArg                *arg,
					   guint                  arg_id);

static void   gnome_canvas_pimage_update      (GnomeCanvasItem *item,
					       double *affine, ArtSVP *clip_path, int flags);
static void   gnome_canvas_pimage_realize     (GnomeCanvasItem *item);
static void   gnome_canvas_pimage_unrealize   (GnomeCanvasItem *item);
static void   gnome_canvas_pimage_draw        (GnomeCanvasItem *item, GdkDrawable *drawable,
					      int x, int y, int width, int height);
static double gnome_canvas_pimage_point       (GnomeCanvasItem *item, double x, double y,
					      int cx, int cy, GnomeCanvasItem **actual_item);
static void   gnome_canvas_pimage_bounds      (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2);
static void   gnome_canvas_pimage_render      (GnomeCanvasItem *item, GnomeCanvasBuf *buf);

static GnomeCanvasItemClass *parent_class;


GtkType
gnome_canvas_pimage_get_type (void)
{
	static GtkType pimage_type = 0;

	if (!pimage_type) {
		GtkTypeInfo pimage_info = {
			"GnomeCanvasPImage",
			sizeof (GnomeCanvasPImage),
			sizeof (GnomeCanvasPImageClass),
			(GtkClassInitFunc) gnome_canvas_pimage_class_init,
			(GtkObjectInitFunc) gnome_canvas_pimage_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		pimage_type = gtk_type_unique (gnome_canvas_item_get_type (), &pimage_info);
	}

	return pimage_type;
}

static void
gnome_canvas_pimage_class_init (GnomeCanvasPImageClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("GnomeCanvasPImage::image", GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_IMAGE);
	gtk_object_add_arg_type ("GnomeCanvasPImage::x", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X);
	gtk_object_add_arg_type ("GnomeCanvasPImage::y", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y);
	gtk_object_add_arg_type ("GnomeCanvasPImage::width", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("GnomeCanvasPImage::height", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEIGHT);

	object_class->destroy = gnome_canvas_pimage_destroy;
	object_class->set_arg = gnome_canvas_pimage_set_arg;
	object_class->get_arg = gnome_canvas_pimage_get_arg;

	item_class->update = gnome_canvas_pimage_update;
	item_class->realize = gnome_canvas_pimage_realize;
	item_class->unrealize = gnome_canvas_pimage_unrealize;
	item_class->draw = gnome_canvas_pimage_draw;
	item_class->point = gnome_canvas_pimage_point;
	item_class->bounds = gnome_canvas_pimage_bounds;
	item_class->render = gnome_canvas_pimage_render;
}

static void
gnome_canvas_pimage_init (GnomeCanvasPImage *image)
{
	image->x = 0.0;
	image->y = 0.0;
	image->width = 0.0;
	image->height = 0.0;
}

static void
free_pixmap_and_mask (GnomeCanvasPImage *image)
{
	image->cwidth = 0;
	image->cheight = 0;
}

static void
gnome_canvas_pimage_destroy (GtkObject *object)
{
	GnomeCanvasPImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIMAGE (object));

	image = GNOME_CANVAS_PIMAGE (object);

	free_pixmap_and_mask (image);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Get's the image bounds expressed as item-relative coordinates. */
static void
get_bounds_item_relative (GnomeCanvasPImage *image, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	double x, y;

	item = GNOME_CANVAS_ITEM (image);

	/* Get item coordinates */

	x = image->x;
	y = image->y;

	/* Bounds */

	*px1 = x;
	*py1 = y;
	*px2 = x + image->width;
	*py2 = y + image->height;
}

static void
get_bounds (GnomeCanvasPImage *image, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	ArtDRect i_bbox;

	item = GNOME_CANVAS_ITEM (image);

 	get_bounds_item_relative (image, &i_bbox.x0, &i_bbox.y0,
				  &i_bbox.x1, &i_bbox.y1);

	/* add a fudge factor */
	*px1 = i_bbox.x0 - 1;
	*py1 = i_bbox.y0 - 1;
	*px2 = i_bbox.x1 + 1;
	*py2 = i_bbox.y1 + 1;
}

static void
gnome_canvas_pimage_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	GnomeCanvasPImage *image;
	int update;
	int calc_bounds;

	item = GNOME_CANVAS_ITEM (object);
	image = GNOME_CANVAS_PIMAGE (object);

	update = FALSE;
	calc_bounds = FALSE;

	switch (arg_id) {
	case ARG_IMAGE:
		/* The pixmap and mask will be freed when the item is reconfigured */
		image->im = GTK_VALUE_POINTER (*arg);
		gdk_pixbuf_ref (image->im);
		update = TRUE;
		break;

	case ARG_X:
		image->x = GTK_VALUE_DOUBLE (*arg);
		update = TRUE;
		break;

	case ARG_Y:
		image->y = GTK_VALUE_DOUBLE (*arg);
		update = TRUE;
		break;

	case ARG_WIDTH:
		image->width = fabs (GTK_VALUE_DOUBLE (*arg));
		update = TRUE;
		break;

	case ARG_HEIGHT:
		image->height = fabs (GTK_VALUE_DOUBLE (*arg));
		update = TRUE;
		break;

	default:
		break;
	}

	if (update)
		gnome_canvas_item_request_update (item);
}

static void
gnome_canvas_pimage_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasPImage *image;

	image = GNOME_CANVAS_PIMAGE (object);

	switch (arg_id) {
	case ARG_IMAGE:
		GTK_VALUE_POINTER (*arg) = image->im;
		break;

	case ARG_X:
		GTK_VALUE_DOUBLE (*arg) = image->x;
		break;

	case ARG_Y:
		GTK_VALUE_DOUBLE (*arg) = image->y;
		break;

	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = image->width;
		break;

	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = image->height;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
gnome_canvas_pimage_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasPImage *image;
	ArtDRect i_bbox, c_bbox;
	int w, h;

	image = GNOME_CANVAS_PIMAGE (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	free_pixmap_and_mask (image);

	if (image->width != gdk_pixbuf_get_width(image->im)
            && image->height != gdk_pixbuf_get_height(image->im)) {
		GdkPixbuf *tmp;
		tmp = gdk_pixbuf_scale_simple (image->im, image->width, image->height,
                                               GDK_INTERP_BILINEAR);
		g_object_unref (image->im);
		image->im = tmp;
	}

	/* only works for non-rotated, non-skewed transforms */
	image->cwidth = (int) (image->width * affine[0] + 0.5);
	image->cheight = (int) (image->height * affine[3] + 0.5);

	if (image->im)
		image->need_recalc = TRUE;

	get_bounds_item_relative (image, &i_bbox.x0, &i_bbox.y0, &i_bbox.x1, &i_bbox.y1);
	art_drect_affine_transform (&c_bbox, &i_bbox, affine);

	/* these values only make sense in the non-rotated, non-skewed case */
	image->cx = c_bbox.x0;
	image->cy = c_bbox.y0;

	/* add a fudge factor */
	c_bbox.x0--;
	c_bbox.y0--;
	c_bbox.x1++;
	c_bbox.y1++;

	gnome_canvas_update_bbox (item, c_bbox.x0, c_bbox.y0, c_bbox.x1, c_bbox.y1);

	if (image->im) {
		w = gdk_pixbuf_get_width (image->im);
		h = gdk_pixbuf_get_height (image->im);
	} else
		w = h = 1;

	image->affine[0] = (affine[0] * image->width) / w;
	image->affine[1] = (affine[1] * image->height) / h;
	image->affine[2] = (affine[2] * image->width) / w;
	image->affine[3] = (affine[3] * image->height) / h;
	image->affine[4] = i_bbox.x0 * affine[0] + i_bbox.y0 * affine[2] + affine[4];
	image->affine[5] = i_bbox.x0 * affine[1] + i_bbox.y0 * affine[3] + affine[5];
}

static void
gnome_canvas_pimage_realize (GnomeCanvasItem *item)
{
	GnomeCanvasPImage *image;

	image = GNOME_CANVAS_PIMAGE (item);

	if (parent_class->realize)
		(* parent_class->realize) (item);

	image->gc = gdk_gc_new (item->canvas->layout.bin_window);
}

static void
gnome_canvas_pimage_unrealize (GnomeCanvasItem *item)
{
	GnomeCanvasPImage *image;

	image = GNOME_CANVAS_PIMAGE (item);

	g_object_unref (image->gc);
	image->gc = NULL;

	if (parent_class->unrealize)
		(* parent_class->unrealize) (item);
}

static void
recalc_if_needed (GnomeCanvasPImage *image)
{
	if (!image->need_recalc)
		return;

	get_bounds (image, &image->item.x1, &image->item.y1, &image->item.x2, &image->item.y2);

	image->need_recalc = FALSE;
}

static void
gnome_canvas_pimage_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			 int x, int y, int width, int height)
{
	GnomeCanvasPImage *image;

	image = GNOME_CANVAS_PIMAGE (item);

	if (!image->im)
		return;

	recalc_if_needed (image);

	if (image->im)
		gdk_draw_pixbuf (drawable,
				 image->gc,
				 image->im,
				 0, 0,
				 image->cx - x,
				 image->cy - y,
				 image->cwidth,
				 image->cheight,
                                 GDK_RGB_DITHER_NORMAL, 0, 0);
}

static double
dist_to_mask (GnomeCanvasPImage *image, int cx, int cy)
{
	GnomeCanvasItem *item;
	GdkImage *gimage;
	GdkRectangle a, b, dest;
	int x, y, tx, ty;
	double dist, best;
        GdkBitmap *mask;

	item = GNOME_CANVAS_ITEM (image);

	/* Trivial case:  if there is no mask, we are inside */

	if (image->im == NULL)
		return 0.0;

	/* Rectangle that we need */

	cx -= image->cx;
	cy -= image->cy;

	a.x = cx - item->canvas->close_enough;
	a.y = cy - item->canvas->close_enough;
	a.width = 2 * item->canvas->close_enough + 1;
	a.height = 2 * item->canvas->close_enough + 1;

	/* Image rectangle */

	b.x = 0;
	b.y = 0;
	b.width = image->cwidth;
	b.height = image->cheight;

	if (!gdk_rectangle_intersect (&a, &b, &dest))
		return a.width * a.height; /* "big" value */

	/* Find the closest pixel */

	best = a.width * a.height; /* start with a "big" value */

        gdk_pixbuf_render_pixmap_and_mask (image->im, NULL, &mask, 127);
        
        if (mask != NULL) {
		gimage = gdk_image_get (mask, dest.x, dest.y,
					dest.width, dest.height);
		g_object_unref (mask);
		if (gimage != NULL) {
			for (y = 0; y < dest.height; y++)
				for (x = 0; x < dest.width; x++)
					if (gdk_image_get_pixel (gimage, x, y)) {
						tx = x + dest.x - cx;
						ty = y + dest.y - cy;
						
						dist = sqrt (tx * tx + ty * ty);
						if (dist < best)
							best = dist;
					}
		gdk_image_destroy (gimage);
		}
	}

	return best;
}

static double
gnome_canvas_pimage_point (GnomeCanvasItem *item, double x, double y,
			  int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasPImage *image;
	int x1, y1, x2, y2;
	int dx, dy;

	image = GNOME_CANVAS_PIMAGE (item);

	*actual_item = item;

	recalc_if_needed (image);

	x1 = image->cx - item->canvas->close_enough;
	y1 = image->cy - item->canvas->close_enough;
	x2 = image->cx + image->cwidth - 1 + item->canvas->close_enough;
	y2 = image->cy + image->cheight - 1 + item->canvas->close_enough;

	/* Hard case: is point inside image's gravity region? */

	if ((cx >= x1) && (cy >= y1) && (cx <= x2) && (cy <= y2))
		return dist_to_mask (image, cx, cy) / item->canvas->pixels_per_unit;

	/* Point is outside image */

	x1 += item->canvas->close_enough;
	y1 += item->canvas->close_enough;
	x2 -= item->canvas->close_enough;
	y2 -= item->canvas->close_enough;

	if (cx < x1)
		dx = x1 - cx;
	else if (cx > x2)
		dx = cx - x2;
	else
		dx = 0;

	if (cy < y1)
		dy = y1 - cy;
	else if (cy > y2)
		dy = cy - y2;
	else
		dy = 0;

	return sqrt (dx * dx + dy * dy) / item->canvas->pixels_per_unit;
}

static void
gnome_canvas_pimage_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasPImage *image;

	image = GNOME_CANVAS_PIMAGE (item);

	*x1 = image->x;
	*y1 = image->y;

	*x2 = *x1 + image->width;
	*y2 = *y1 + image->height;
}

static void
gnome_canvas_pimage_render      (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	GnomeCanvasPImage *image;

	image = GNOME_CANVAS_PIMAGE (item);

        gnome_canvas_buf_ensure_buf (buf);

	buf->is_bg = 0;
}
