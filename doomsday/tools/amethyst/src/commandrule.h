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

#ifndef __AMETHYST_COMMAND_RULE_H__
#define __AMETHYST_COMMAND_RULE_H__

#include "gemclass.h"
#include "linkable.h"
#include "utils.h"

// Command rule flags.
#define CRF_INDEPENDENT         0x01
#define CRF_BREAKING            0x02 // Make a paragraph break before the command.
#define CRF_LINE_BREAKING       0x04 // Make a line break before the command.
#define CRF_POST_BREAKING       0x08 // Make a paragraph break after the command.
#define CRF_POST_LINE_BREAKING  0x10 // Make a line break after the command.
#define CRF_TIDY                0x20 // Doesn't generate shards.

#define CRF_DEFAULT             (CRF_INDEPENDENT|CRF_BREAKING)

class CommandRule : public Linkable
{
public:
    CommandRule();
    CommandRule(const String& _name, const GemClass &gc, int _flags = 0, const String& args = "");

    // Getters.
    CommandRule *next() { return (CommandRule*) Linkable::next(); }
    CommandRule *prev() { return (CommandRule*) Linkable::prev(); }
    const String& name() const { return _name; }
    int flags() const { return _flags; }
    const GemClass &gemClass() const { return _gemClass; }
    GemClass &gemClass() { return _gemClass; }
    const String& argTypes() const { return _argTypes; }

    // Setters.
    void setName(const String& str) { _name = str; }
    int modifyFlags(int setFlags = 0, int clearFlags = 0);
    void setGemClass(const GemClass &gc) { _gemClass = gc; }
    void setArgTypes(const String& str) { _argTypes = str; }

    // Information.
    bool hasFlag(int req) const { return (_flags & req) == req; }
    bool isIndependent() const { return hasFlag(CRF_INDEPENDENT); }
    bool isBreaking() const { return hasFlag(CRF_BREAKING); }
    bool isLineBreaking() const { return hasFlag(CRF_LINE_BREAKING); }
    bool isGemType(GemClass::GemType type) const { return _gemClass.type() == type; }
    ArgType argType(int zeroBasedIndex = 0) const;
    
protected:
    String          _name;
    int             _flags;
    GemClass        _gemClass;  // For independent commands.
    String          _argTypes;
};

#endif
