/* Yo Emacs, this -*- C++ -*-

  Copyright (C) 1999,2000 Jens Hoefkens
  jens@hoefkens.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
*/

#ifndef __KBGTEXTVIEW_H 
#define __KBGTEXTVIEW_H 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <ktextbrowser.h>
#include <qstring.h>


/**
 * A small extension to the QTextView control.
 */
class KBgTextView : public KTextBrowser
{
	Q_OBJECT

public:

	/**
	 * Constructor
	 */
	KBgTextView(QWidget *parent = 0, const char *name = 0);

	/**
	 * Destructor
	 */
	virtual ~KBgTextView();

public slots:

	/**
	 * Restore previously saved setting or provides defaults
	 */
        void readConfig();

	/**
	 * Save current settings
	 */
	void saveConfig();

        /**
	 * Simple interface to the non-slot function selectFont()
	 */
        void selectFont();

	/**
	 * Clears the view by setting the text to ""
	 */
	void clear();

	/**
	 * Write the string at the end of the buffer and scroll to 
	 * the end
	 */
	void write(const QString &);
};

#endif // __KBGTEXTVIEW_H 
