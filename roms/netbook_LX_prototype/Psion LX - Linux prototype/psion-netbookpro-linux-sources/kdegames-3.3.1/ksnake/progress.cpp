/**
 * This file is part of the ksnake package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "progress.h"

Progress::Progress(QWidget *parent, const char *name)
	: KGameProgress(0, 300, 300, KGameProgress::Horizontal, parent, name)
{
  setBarColor("green1");
  setTextEnabled(FALSE);
}

void Progress::advance()
{
  if (value() == 0) {
    emit restart();
    return;
  }

  if (value() == 80)
    setBarColor("red1");

  KGameProgress::advance(-1);
}

void Progress::rewind()
{
  setBarColor("green1");
  KGameProgress::setValue(300);
}

#include "progress.moc"
