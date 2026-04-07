/** @file aboutdialog.h  Dialog for information about the program.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <de/term/dialogwidget.h>

/**
 * Dialog for information about the program.
 */
class AboutDialog : public de::term::DialogWidget
{
public:
    AboutDialog();

    bool handleEvent(const de::Event &event);
};

#endif // ABOUTDIALOG_H
