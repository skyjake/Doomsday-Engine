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

#include "outputcontext.h"
#include "gem.h"

OutputContext::OutputContext() : Linkable()
{
    _leftEdge = 0;
    _rightEdge = 71;
    _cursor = 0;
    _align = AlignLeft;
    _gem = _top = 0;
    _full = false;
}

OutputContext::OutputContext(const OutputContext &other) : Linkable()
{
    initWith(other);
}

void OutputContext::initWith(const OutputContext &other)
{
    _leftEdge = other._leftEdge;
    _rightEdge = other._rightEdge;
    _cursor = _leftEdge;
    _align = other._align;
    _gem = other._gem;
    _top = other._top;
    _full = other._full;
}

/**
 * Get the logically following gem, but don't rise above the top.
 */
Gem *OutputContext::nextPos(bool canDescend)
{
    if(!_gem) return 0;
    if(canDescend && _gem->firstGem())
        return _gem = _gem->firstGem(); // Step into.
    if(_gem->nextGem()) return _gem = _gem->nextGem();      // Step forward.
    while(_gem->parentGem())
    {
        // Step upwards...
        _gem = _gem->parentGem();
        if(_gem == _top) break;
        if(_gem->nextGem()) return _gem = _gem->nextGem(); // And onward.
    }
    // Out of gems.
    return _gem = 0;
}

bool OutputContext::roomFor(const String& str)
{
    if(str.isEmpty()) return true;
    // Account for one character for the space.
    bool fits = (_cursor == _leftEdge) || (_cursor + int(str.size()) - 1 <= _rightEdge);
    if(!fits) _full = true;
    return fits;
}

void OutputContext::print(const String& str)
{
    if(!str.isEmpty())
    {
        _output += str;
        _cursor += str.size();
    }
}

void OutputContext::setAlignMode(AlignMode mode)
{
    if(mode == _align) return; // No change.
    _align = mode;
    // Print a control sequence that will guide the renderer.
    String ctlSeq;
    ctlSeq += CtrlAlign;
    ctlSeq += 
          mode == AlignLeft? 'L' 
        : mode == AlignRight? 'R' 
        : 'C';
    print(ctlSeq);
}
