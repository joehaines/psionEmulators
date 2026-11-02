/* -*- mode:C; indent-tabs-mode:t; tab-width:8; c-basic-offset:2; -*- */

/* gnome-stones - object.c
 *
 * Time-stamp: <2003/06/19 21:12:50 mccannwj>
 *
 * Copyright (C) 1998 Carsten Schaar
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

#include "object.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>



/*****************************************************************************/

/* Hash table of all loaded plugins.  */
static GHashTable *plugin_table= NULL;


/* Initialize a plugin.  At least the object_init element should be
   set.  

   This functions call the 'object_init' function, initializes the
   object hash table.  */
static gboolean
plugin_init (GStonesPlugin *plugin)
{
  gchar *title;
  
  /* Initialize the plugin's object table.  */
  plugin->objects= g_hash_table_new (g_str_hash, g_str_equal);
  if (!plugin->objects)
    {
      printf ("Unable to allocate object table\n");
      return FALSE;
    }
  
  /* Call 'object_init' function.  */
  title= plugin->objects_init (plugin);
  if (title == NULL)
    {
      printf ("objects_init returned error");
      return FALSE;
    }

  plugin->title= g_strdup (title);
  if (plugin->title == NULL)
    {
      printf ("not enough memory");
      return FALSE;
    }

  if (!plugin_table)
    {
      plugin_table= g_hash_table_new (g_str_hash, g_str_equal);
      /* FIXME: add error handling here.  */
    }

  /* Add plugin to hash table of all plugins.  */
  g_hash_table_insert (plugin_table, plugin->title, plugin);

  return TRUE;
}


/* Load a plugin.  */

GStonesPlugin *
plugin_load (const gchar *filename)
{
  GStonesPlugin *plugin;
  
  g_return_val_if_fail (filename != NULL, NULL);
  
  plugin= g_new0 (GStonesPlugin, 1);
  if (!plugin)
    {
      g_print ("allocation error");
      return NULL;
    }

  plugin->handle= g_module_open (filename, 0);
  if (!plugin->handle) 
    {
      printf ("Unable to open module file: %s\n", g_module_error());
      g_free (plugin);
      return NULL;
    }
  
  if (!g_module_symbol (plugin->handle, "objects_init", 
			(gpointer *) &plugin->objects_init))
    {
      printf ("Plugin must contain objects_init function.");
      goto error;
    }

  if (!plugin_init (plugin))
    {
      goto error;
    }

  return plugin;
  
 error:
  g_module_close (plugin->handle);
  g_free (plugin);
  return NULL;
}


#define SOEXT ("." G_MODULE_SUFFIX)
#define SOEXT_LEN (strlen (SOEXT))

void
plugin_load_plugins_in_dir (const gchar *directory)
{
  GDir *d;
  G_CONST_RETURN gchar *e;
  
  if ((d = g_dir_open (directory, 0, NULL)) == NULL)
    return;
  
  while ((e = g_dir_read_name (d)) != NULL)
    {
      if ((strlen (e) > SOEXT_LEN) &&  
	  (strncmp (e + strlen (e) - SOEXT_LEN, SOEXT, SOEXT_LEN) == 0))
	{
	  gchar *objects_name;
	  
	  objects_name = g_build_filename (directory, e, NULL);
	  plugin_load (objects_name);
	  g_free (objects_name);
	}
    }
  g_dir_close (d);
}


GStonesPlugin *
plugin_find_by_title (const gchar *title)
{
  if (!plugin_table)
    return NULL;
  
  return g_hash_table_lookup (plugin_table, title);
}



/*****************************************************************************/
/* We need at least one object.  The FRAME object.  */


static GStonesObjectDesc frame_description=
{
  "frame",

  NULL,
  NULL,
  
  NULL,
  NULL,
  
  "frame.png",
  NULL,
  0, 0
};

/*****************************************************************************/
/* This is animated curtain.  */


static GStonesObjectDesc curtain_description=
{
  "curtain",

  NULL,
  NULL,
  
  NULL,
  NULL,
  
  "animated_curtain.png",
  NULL,
  0, 0
};


static gchar *
default_objects_init (GStonesPlugin *plugin)
{
  object_register (plugin, &frame_description);
  object_register (plugin, &curtain_description);

  return "default";

}


static GStonesPlugin default_plugin=
{
  NULL,
  NULL,
  NULL,
  default_objects_init
};


static void
default_objects_register (void)
{
  /* The frame object has only to be registered once.  */
  if (default_plugin.objects)
    return;

  plugin_init (&default_plugin);
}



/*****************************************************************************/
/* Object handling.  */


GStonesObject *
object_register (GStonesPlugin *plugin, GStonesObjectDesc *description)
{
  guint          num_x;
  guint          num_y;
  guint          x;
  guint          y;
  guint          idx;
  gchar          *filename;
  gchar          *pathname;
  GdkPixbuf *image;
  GStonesObject *object;

  object= g_new0 (GStonesObject, 1);
  /* Return 'NULL' if there is no memory left.  */
  if (!object)
    return NULL;

  object->description= description;
  object->plugin     = plugin;

  filename= g_build_filename ("gnome-stones", description->image_name, NULL);
  pathname= gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
                                       filename, TRUE, NULL);
  image   = gdk_pixbuf_new_from_file (pathname, NULL);
  g_free (pathname);
  g_free (filename);
  
  if (!image)
    {
      gchar *error= g_strdup_printf ("Error while loading image %s: file not found!", 
                                    description->image_name);
      g_warning (error);
      g_free (error);

      return NULL;
    }

  /* Determine the number of images in the loaded image.  */
  num_x= gdk_pixbuf_get_width (image)/STONE_SIZE;
  num_y= gdk_pixbuf_get_height (image)/STONE_SIZE;

  if (num_x*num_y == 0)
    {
      gchar *error= g_strdup_printf ("Error while registering object %s: image contains no data!", 
                                     description->image_name);
      g_warning (error);
      g_free (error);

      return NULL;
    }

  object->num_images= num_x*num_y;
  object->image= g_malloc (num_x*num_y*sizeof (GdkPixmap*));
  
  if (object->image == NULL)
    {
      char *error= g_strdup_printf ("Error while registering object %s: out of memory!", 
                                    description->image_name);
      g_warning (error);
      g_free (error);

      return NULL;
    }
  
  object->pixbuf_image= g_malloc (num_x*num_y*sizeof (GdkPixbuf*));
  
  if (object->pixbuf_image == NULL)
    {
      char *error= g_strdup_printf ("Error while registering object %s: out of memory!", 
                                    description->image_name);
      g_warning (error);
      g_free (error);
      g_free (object->image);

      return NULL;
    }
  
  /* We cut our image into small pieces.  */
  /* gdk_pixbuf_render (image, image->rgb_width, image->rgb_height); */

  idx= 0;
  for (y= 0; y < num_y; y++)
    for (x= 0; x < num_x; x++, idx++)
      {
	/* FIXME: check error conditions.  */
	object->pixbuf_image[idx]= 
	   gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, STONE_SIZE, STONE_SIZE);
	gdk_pixbuf_copy_area (image, x*STONE_SIZE, y*STONE_SIZE,
                                          STONE_SIZE, STONE_SIZE, object->pixbuf_image[idx], 0, 0);
	
	gdk_pixbuf_render_pixmap_and_mask (object->pixbuf_image[idx], &object->image[idx], NULL, 127);
      }
  
  /* Add object to the plugin's object table.  */
  g_hash_table_insert (plugin->objects, description->name, object);

  return object;
}


GStonesObject *
object_find_object_by_name (const gchar *name)
{
  gchar         **parts = NULL;
  GStonesObject  *object= NULL;
  
  /* Check arguments.  */
  g_return_val_if_fail (name, NULL);

  /* We have to register the default objects first.  */
  default_objects_register ();

  /* The object's name must be given in the form 'plugin:object', so we
     start parsing the name.  */
  parts= g_strsplit (name, ":", 3);
  
  if (parts && parts[0] && parts[1] && !parts[2])
    {
      GStonesPlugin *plugin= g_hash_table_lookup (plugin_table, parts[0]);
      
      if (plugin)
	object= g_hash_table_lookup (plugin->objects, parts[1]);
    }
  
  g_strfreev (parts);
  
  return object;
}


GdkPixmap *
object_get_image (GStonesObject *object, gint index)
{
  g_return_val_if_fail (object, NULL);
  /* FIXME: add range checks.  */
  
  if (index == OBJECT_DEFAULT_IMAGE)
    index= object->image_index;
  else if (index == OBJECT_EDITOR_IMAGE)
    index= object->editor_index;
  
  g_return_val_if_fail ((index >= 0) && (index <= object->num_images), NULL);

  return object->image[index];
}


GdkPixbuf *
object_get_pixbuf_image (GStonesObject *object, gint index)
{
  g_return_val_if_fail (object, NULL);
  
  if (index == OBJECT_DEFAULT_IMAGE)
    index= object->image_index;
  else if (index == OBJECT_EDITOR_IMAGE)
    index= object->editor_index;
  
  g_return_val_if_fail ((index >= 0) && (index <= object->num_images), NULL);

  return object->pixbuf_image[index];
}


gchar *
object_get_fullname (GStonesObject *object)
{
  g_return_val_if_fail (object, NULL);

  return g_strconcat (object->plugin->title, ":", 
			 object->description->name, NULL);
}

/*****************************************************************************/
/* Object context definitions.  */


struct _GStonesObjContext
{
  /* The object's description data.  */
  GStonesObjectDesc *description;

  /* The options.  */
  GHashTable        *options;

  /* Some private data.  */
  gpointer           private;
};


/* general */
GStonesObjContext *
object_context_new (GStonesObject *object)
{
  GStonesObjContext *context;

  g_return_val_if_fail (object, NULL);
  
  context= g_new0 (GStonesObjContext, 1);
  if (!context)
    return NULL;
 
  context->description= object->description;

  context->options    = g_hash_table_new (g_str_hash, g_str_equal);
  if (!context->options)
    {
      g_free (context);
      return NULL;
    }
  
  return context;
}


static gboolean
object_context_free_option (gchar *name, gchar *value, gpointer user)
{
  g_free (name);
  g_free (value);
  
  return TRUE;
}

void
object_context_free (GStonesObjContext *context)
{
  g_return_if_fail (context);
  
  g_hash_table_foreach_remove 
    (context->options, (GHRFunc) object_context_free_option, NULL);

  g_free (context);
}


/* Setting and getting private data.  */
void     
object_context_set_private_data (GStonesObjContext *context,
				 gpointer           private)
{
  g_return_if_fail (context);
  
  context->private= private;  
}

gpointer 
object_context_private_data (GStonesObjContext *context)
{
  g_return_val_if_fail (context, NULL);

  return context->private;
}


/* Setting and getting options.  */

gboolean
object_context_set_option (GStonesObjContext *context,
			   const gchar       *name,
			   const gchar       *value)
{
  gchar *old_value;
  gchar *old_name;

  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (name, FALSE);
  
  if (g_hash_table_lookup_extended (context->options,
				    name,
				    (gpointer) &old_name,
				    (gpointer) &old_value))
    {
      /* Remove old options.  */
      g_hash_table_remove (context->options, name);
      g_free (old_name);
      g_free (old_value);
    }
  
  if (value)
    {
      /* Set a new option.  */
      g_hash_table_insert 
	(context->options, g_strdup (name), g_strdup (value));
    }

  /* FIXME: Add empty memory checks here.  */
  return TRUE;
}


/* A small helper function, needed by
   'object_context_get_string_option'.  */
static GStonesObjectOption *
find_option_by_name (GStonesObjectDesc *desc, const gchar *name)
{
  GStonesObjectOption *option;
  gchar               *tmp_name;  
  
  option= desc->options;

  /* Check, if this object has any options.  */
  if (!option)
    return NULL;

  tmp_name= g_strconcat (name, "=", NULL);

  while (option->name)
    {
      if (strncmp (option->name, tmp_name, strlen (tmp_name)) == 0)
	break;

      /* Check next option.  */
      option++;
    }  

  g_free (tmp_name);

  if (option->name)
    return option;
  else
    return NULL;
}


gchar *
object_context_get_string_option (GStonesObjContext *context,
				  const gchar       *name)
{
  gchar *value;

  g_return_val_if_fail (context, NULL);
  g_return_val_if_fail (name, NULL);
  
  value= g_hash_table_lookup (context->options, name);

  if (value)
    return g_strdup (value);
  else
    {
      /* We try to get the option's default value.  */
      gchar *temp;
      GStonesObjectOption *option= find_option_by_name 
	(context->description, name);
  
      if (!option)
	return NULL;
  
      temp= strchr (option->name, '=');
      /* We can be sure, that tmp is not equal to 'NULL', because otherwise 
	 'object_find_option_by_name' would not have found this option.  */
      temp++;
      return g_strdup (temp);
    }
}

gboolean 
object_context_get_bool_option (GStonesObjContext *context,
				const gchar       *name)
{
  gchar *value= object_context_get_string_option (context, name);
  
  if (value)
    {
      gboolean v= FALSE;
      
      if (!strcasecmp (value, "true"))
	{
	  v= TRUE;
	}
      else if (!strcasecmp (value, "1"))
	{
	  v= TRUE;
	} 

      g_free (value);
      
      return v;
    }
  
  return FALSE;
}

gint     
object_context_get_int_option (GStonesObjContext *context,
			       const gchar       *name)
{
  gchar *value= object_context_get_string_option (context, name);
  
  if (value)
    {
      /* FIXME: Set locale here?  */
      int v= atoi (value);
      
      g_free (value);
      
      return v;
    }

  return 0;
}

gdouble
object_context_get_float_option (GStonesObjContext *context,
				 const gchar       *name)
{
  gchar *value= object_context_get_string_option (context, name);
  
  if (value)
    {
      /* FIXME: Set locale here?  */
      gdouble v= strtod (value, NULL);
      
      g_free (value);
      
      return v;
    }

  return 0;
}


/* Local Variables: */
/* mode:c */
/* eval:(load-library "time-stamp") */
/* eval:(make-local-variable 'write-file-hooks) */
/* eval:(add-hook 'write-file-hooks 'time-stamp) */
/* eval:(setq time-stamp-format '(time-stamp-yyyy/mm/dd time-stamp-hh:mm:ss user-login-name)) */
/* End: */

