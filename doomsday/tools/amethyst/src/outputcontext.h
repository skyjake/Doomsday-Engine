/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AMETHYST_OUTPUT_CONTEXT_H__
#define __AMETHYST_OUTPUT_CONTEXT_H__

#include "string.h"
#include "linkable.h"
#include "gem.h"

class OutputContext : public Linkable
{
public:
    enum AlignMode { AlignLeft, AlignRight, AlignCenter };
    enum ControlChar {
        CtrlAlign = 1,
        CtrlFill = 2,
        CtrlUnderfill = 3,
        CtrlLinePrefixBegin = 4,
        CtrlLinePrefixEnd = 5,
        CtrlCleanBreaks = 6,    // remove duplicate breaks
        CtrlRawBreaks = 7,      // output breaks as-is
        CtrlAnchor = 8,         // position marker for anchor output
        CtrlBreakingSpace = 9,  // \t
        CtrlParagraphBreak = 10,// \n
        CtrlAnchorPrepend = 11,
        CtrlAnchorAppend = 12,
        CtrlLineBreak = 13,     // \r
        CtrlTab = 14
    };

public:
    OutputContext();
    OutputContext(const OutputContext &other);

    void initWith(const OutputContext &other);
    bool roomFor(const String& str);
    void print(const String& str);
    bool isFull() { return _full; }
    bool isEmpty() { return _output.isEmpty(); }

    OutputContext *next() { return (OutputContext*) Linkable::next(); }
    OutputContext *prev() { return (OutputContext*) Linkable::prev(); }

    void setTop(Gem *g) { _top = g; }
    Gem *top() { return _top; }
    Gem *pos() { return _gem; }
    void setPos(Gem *pos) { _gem = pos; }
    void startFrom(Gem *g) { _top = g; _gem = g? g->firstGem() : 0; }
    Gem *nextPos(bool canDescend = true);
    bool hasGems() { return _gem != 0; }

    const String& output() { return _output; }
    int cursor() { return _cursor; }
    int leftEdge() { return _leftEdge; }
    int rightEdge() { return _rightEdge; }
    void setLeftEdge(int coord) { _leftEdge = _cursor = coord; }
    void setRightEdge(int coord) { _rightEdge = coord; }
    void moveLeftEdge(int amount) { _cursor = _leftEdge += amount; }
    void moveRightEdge(int amount) { _rightEdge += amount; }
    int width() { return _rightEdge - _leftEdge + 1; }
    void setWidth(int newWidth) { _rightEdge = _leftEdge + newWidth - 1; }
    
    void setAlignMode(AlignMode mode);
    AlignMode alignMode() { return _align; }

    friend class Schedule;

private:
    int     _leftEdge, _rightEdge;
    int     _cursor;
    AlignMode _align;
    bool    _full;
    String  _output;
    Gem *_gem, *_top;
};

#endif
