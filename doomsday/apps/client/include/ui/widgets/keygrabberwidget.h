/** @file keygrabberwidget.h  Grabs key events and shows them.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_KEYGRABBERWIDGET_H
#define DE_CLIENT_KEYGRABBERWIDGET_H

#include <de/labelwidget.h>

/**
 * When focused, grabs key events and shows the key event data. However, Esc is
 * never grabbed.
 */
class KeyGrabberWidget : public de::LabelWidget
{
public:
    KeyGrabberWidget(const de::String &name = de::String());

    bool handleEvent(const de::Event &event);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_KEYGRABBERWIDGET_H
