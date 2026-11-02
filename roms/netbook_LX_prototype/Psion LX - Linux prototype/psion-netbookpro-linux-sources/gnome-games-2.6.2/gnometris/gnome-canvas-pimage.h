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
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2001 Juan Pablo Mendoza Mendoza
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx> (Original code).
 *          Juan Pablo Mendoza <pablo_juan@yahoo.com>
 */

#ifndef GNOME_CANVAS_PIMAGE_H
#define GNOME_CANVAS_PIMAGE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libgnomecanvas/gnome-canvas.h>



#define GNOME_TYPE_CANVAS_PIMAGE            (gnome_canvas_pimage_get_type ())
#define GNOME_CANVAS_PIMAGE(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_CANVAS_PIMAGE, GnomeCanvasPImage))

#define GNOME_CANVAS_PIMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GNOME_TYPE_CANVAS_PIMAGE, GnomeCanvasPImageClass))
#define GNOME_IS_CANVAS_PIMAGE(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_CANVAS_PIMAGE))
#define GNOME_IS_CANVAS_PIMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_PIMAGE))


typedef struct _GnomeCanvasPImage GnomeCanvasPImage;
typedef struct _GnomeCanvasPImageClass GnomeCanvasPImageClass;

struct _GnomeCanvasPImage {
	GnomeCanvasItem item;

	GdkPixbuf *im;			/* The image to paint */

	double x, y;			/* Position at anchor, item relative */
	double width, height;		/* Size of image, item relative */

	int cx, cy;			/* Top-left canvas coordinates for display */
	int cwidth, cheight;		/* Rendered size in pixels */
	GdkGC *gc;			/* GC for drawing image */

	unsigned int need_recalc : 1;	/* Do we need to rescale the image? */

	double affine[6];               /* The item -> canvas affine */
};

struct _GnomeCanvasPImageClass {
	GnomeCanvasItemClass parent_class;
};


/* Standard Gtk function */
GtkType gnome_canvas_pimage_get_type (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
