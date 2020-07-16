/** @file ilinewrapping.h
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "cstring.h"

namespace de {

using WrapWidth = duint;

/**
 * Line of word-wrapped text.
 */
struct DE_PUBLIC WrappedLine
{
    CString   range;
    WrapWidth width;
    bool      isFinal;

    WrappedLine(const CString &range, WrapWidth width, bool final = false)
        : range(range)
        , width(width)
        , isFinal(final)
    {}
};

class DE_PUBLIC ILineWrapping
{
public:
    virtual ~ILineWrapping() = default;

    virtual bool        isEmpty() const                                         = 0;
    virtual void        clear()                                                 = 0;
    virtual void        wrapTextToWidth(const String &text, WrapWidth maxWidth) = 0;
    virtual WrappedLine line(int index) const                                   = 0;

    /// Determines the visible maximum width of the wrapped content.
    virtual WrapWidth width() const = 0;

    /// Determines the number of lines in the wrapped content.
    virtual int height() const = 0;

    /// Returns the advance width of the range.
    virtual WrapWidth rangeWidth(const CString &range) const = 0;

    /**
     * Calculates which index in the text content occupies a character at a given width.
     *
     * @param range  Range within the content.
     * @param width  Advance width to check.
     *
     * @return Index from the beginning of the content (note: @em NOT the beginning
     * of @a range).
     */
    virtual BytePos indexAtWidth(const CString &range, WrapWidth width) const = 0;
};

} // namespace de
