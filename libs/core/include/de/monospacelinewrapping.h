/** @file monospacelinewrapping.h
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

#pragma once

#include "de/ilinewrapping.h"
#include "de/list.h"

namespace de {

/**
 * Line wrapper that assumes that all characters are the same width. Width is
 * defined in characters, height in lines.
 *
 * @ingroup textUi
 */
class DE_PUBLIC MonospaceLineWrapping : public ILineWrapping
{
public:
    MonospaceLineWrapping();

    bool isEmpty() const override;
    void clear() override;

    /**
     * Determines word wrapping for a line of text given a maximum line width.
     *
     * @param text      Text to wrap.
     * @param maxWidth  Maximum width for each text line.
     *
     * @return List of positions in @a text where to break the lines. Total number
     * of word-wrapped lines is equal to the size of the returned list.
     */
    void wrapTextToWidth(const String &text, WrapWidth maxWidth) override;

    WrappedLine line(int index) const override { return _lines[index]; }
    WrapWidth   width() const override;
    int         height() const override;
    WrapWidth   rangeWidth(const CString &range) const override;
    BytePos     indexAtWidth(const CString &range, WrapWidth width) const override;

private:
    String            _text;
    List<WrappedLine> _lines;
};

} // namespace de
