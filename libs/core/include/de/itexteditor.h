/** @file itexteditor.h
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBSHELL_ITEXTEDITOR_H
#define LIBSHELL_ITEXTEDITOR_H

#include "de/string.h"

namespace de {

/**
 * Interface for a text editor: text content and a cursor position.
 *
 * @ingroup abstractUi
 */
class ITextEditor
{
public:
    virtual ~ITextEditor() = default;

    virtual void setText(const String &text) = 0;
    virtual void setCursor(BytePos bytePos)  = 0;

    virtual String  text() const   = 0;
    virtual BytePos cursor() const = 0;
};

} // namespace de

#endif // LIBSHELL_ITEXTEDITOR_H
