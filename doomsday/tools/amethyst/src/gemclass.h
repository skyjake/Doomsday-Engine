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

#ifndef __AMETHYST_GEM_CLASS_H__
#define __AMETHYST_GEM_CLASS_H__

#include "string.h"
#include "length.h"

// Style flags.
#define GSF_EMPHASIZE       0x00000001
#define GSF_DEFINITION      0x00000002
#define GSF_CODE            0x00000004
#define GSF_KEYBOARD        0x00000008
#define GSF_SAMPLE          0x00000010
#define GSF_VARIABLE        0x00000020
#define GSF_FILE            0x00000040
#define GSF_OPTION          0x00000080
#define GSF_COMMAND         0x00000100
#define GSF_CITE            0x00000200
#define GSF_ACRONYM         0x00000400
#define GSF_URL             0x00000800
#define GSF_EMAIL           0x00001000
#define GSF_STRONG          0x00002000
#define GSF_ENUMERATE       0x00004000
#define GSF_HEADER          0x00008000
#define GSF_BREAK_LINE      0x00010000
#define GSF_SINGLE          0x00020000
#define GSF_DOUBLE          0x00040000
#define GSF_THICK           0x00080000
#define GSF_THIN            0x00100000
#define GSF_ROMAN           0x00200000
#define GSF_LARGE           0x00400000
#define GSF_SMALL           0x00800000
#define GSF_HUGE            0x01000000
#define GSF_TINY            0x02000000
#define GSF_NOTE            0x04000000
#define GSF_WARNING         0x08000000
#define GSF_IMPORTANT       0x10000000
#define GSF_PREFORMATTED    0x20000000
#define GSF_CAPTION         0x40000000
#define GSF_TAG             0x80000000

#define GS_HIGHEST_TITLE   GemClass::PartTitle
#define GS_LOWEST_TITLE    GemClass::Sub4SectionTitle

class GemClass
{
public:
    enum GemType {
        None,
        Gem,
        Indent,
        List,
        DefinitionList,
        Table,
        PartTitle,
        ChapterTitle,
        SectionTitle,
        SubSectionTitle,
        Sub2SectionTitle,
        Sub3SectionTitle,
        Sub4SectionTitle,
        Contents
    };
    enum FlushMode {
        FlushInherit, // Same as FlushLeft, if not overridden.
        FlushLeft,
        FlushRight,
        FlushCenter
    };

public:
    GemClass(GemType _type = Gem, int _style = 0, FlushMode = FlushInherit, const String& filterString = "");
    GemClass(int _style, FlushMode mode = FlushInherit);

    const Length &length() const { return _length; }
    Length &length() { return _length; }

    GemType type() const { return _type; }
    String typeAsString() const;
    int style() const { return _style; }
    String styleAsString() const;
    FlushMode flushMode() const { return _flush; }
    const String& filter() const { return _filter; }

    // Setters.
    int modifyStyle(int setFlags = 0, int clearFlags = 0);
    void setFlushMode(FlushMode mode) { _flush = mode; }
    void setFilter(const String& flt) { _filter = flt; }

    // Information.
    bool hasStyle(int flags) const;
    bool hasFilter() const { return !_filter.isEmpty(); }
    bool isTitleType()
        { return _type >= GS_HIGHEST_TITLE && _type <= GS_LOWEST_TITLE; }

    GemClass operator + (const GemClass &other) const;

private:
    int         _style;     // Style flags.
    GemType     _type;      // Type of gem.
    FlushMode   _flush;
    String      _filter;
    Length      _length;
};

#endif
