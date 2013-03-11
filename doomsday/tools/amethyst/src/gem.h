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

#ifndef __AMETHYST_GEM_H__
#define __AMETHYST_GEM_H__

#include "string.h"
#include "shard.h"
#include "gemclass.h"

// Gems represent the logical structure of the document.

class Gem : public Shard
{
public:
    Gem(const GemClass& gc, const String& text = "");
    Gem(const String& t = "");
    ~Gem();

    // Accessors.
    const char *cStrText() const { return _text.toUtf8().data(); }
    String text() { return _text; }
    GemClass &gemClass() { return _class; }
    GemClass::GemType gemType() { return _class.type(); }
    int style() { return _class.style(); }
    int modifyStyle(int setFlags, int clearFlags = 0);
    int width() { return _width; }
    void setWidth(int relativeWidth) { _width = relativeWidth; }
    bool isControl() { return _text.isEmpty(); }
    bool isBreak();
    bool isLineBreak() { return isControl() && _class.hasStyle(GSF_BREAK_LINE); }
    String dump();

    Gem *parentGem() { return (Gem*) parent(); }
    Gem *nextGem() { return (Gem*) next(); }
    Gem *prevGem() { return (Gem*) prev(); }
    Gem *firstGem() { return (Gem*) first(); }
    Gem *lastGem() { return (Gem*) last(); }
    Gem *finalGem() { return (Gem*) final(); }
    Gem *followingGem() { return (Gem*) following(); }
    Gem *precedingGem() { return (Gem*) preceding(); }

    Gem *makeBreak(int breakStyle = 0);
    void polish();

protected:
    String      _text;
    int         _width;
    GemClass    _class;
};

#endif
