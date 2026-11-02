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

#include "kbgtextview.moc"
#include "kbgtextview.h"

#include <klocale.h>
#include <kconfig.h>
#include <kcmenumngr.h>
#include <kfontdialog.h>
#include <qfont.h>
#include <kapplication.h>

#include <iostream>

// == advanced text control ====================================================

/*
 * Constructor
 */
KBgTextView::KBgTextView(QWidget *parent, const char *name)
	: KTextBrowser(parent, name)
{
	clear();
	setLinkUnderline(true);
}

/*
 * Destructor
 */
KBgTextView::~KBgTextView()
{
	// empty
}

/*
 * Write the string l to the TextView and put the cursor at the end of
 * the current text
 */
void KBgTextView::write(const QString &l)
{
	append("<font face=\"" + font().family() + "\">" + l + "</font><br>\n");
	scrollToBottom();
}

/*
 * Clears the view by overwriting the text with an empty string.
 */
void KBgTextView::clear()
{
	setText("");
}

/*
 * Open a font-selection dialog.
 */
void KBgTextView::selectFont()
{
	QFont f = font();
	KFontDialog::getFont(f, false, this, true);
	setFont(f);
}

/*
 * Restore the previously stored settings
 */
void KBgTextView::readConfig()
{
	KConfig* config = kapp->config();
	config->setGroup(name());

	// nothing to restore
}

/*
 * Save the current settings to disk
 */
void KBgTextView::saveConfig()
{
	KConfig* config = kapp->config();
	config->setGroup(name());

	// nothing to save
}

// EOF
