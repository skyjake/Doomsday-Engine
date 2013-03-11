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

#include "gem.h"
#include "utils.h"
#include <QTextStream>
#include <QStringList>

Gem::Gem(const GemClass& gc, const String& t) : Shard(Shard::GEM)
{
    _text = t;
    _class = gc;
    _width = 0;
}

Gem::Gem(const String& t) : Shard(Shard::GEM)
{
    _class = GemClass::Gem;
    _text = t;
    _width = 0;
}

Gem::~Gem()
{}

bool Gem::isBreak()
{ 
    return isControl() && _class.type() == GemClass::Gem
        && !first() && !style();
}

int Gem::modifyStyle(int setFlags, int clearFlags)
{
    return _class.modifyStyle(setFlags, clearFlags);
}

Gem *Gem::makeBreak(int breakStyle)
{
    if(!firstGem()) return NULL;
    if(lastGem() &&
            (lastGem()->isBreak()
             || (breakStyle == GSF_BREAK_LINE && lastGem()->isLineBreak())))
        return lastGem();
    return (Gem*) add(new Gem(GemClass(breakStyle)));
}

/**
 * Removes unnecessary/unwanted Break gems.
 */
void Gem::polish()
{
    // Recursively polish the whole gem tree.
    Gem* next = 0;
    for(Gem *it = firstGem(); it; it = next)
    {
        next = it->nextGem();
        it->polish();

        // Remove a line break if it's followed by a paragraph break.
        if(it->isLineBreak() && next && next->isBreak())
            delete remove(it);
    }

    if(firstGem() == lastGem()) return; // Nothing much to do.

    // Remove the last gem if it's a break.
    if(lastGem() && (lastGem()->isBreak() || lastGem()->isLineBreak()))
        delete remove(lastGem());
}

String Gem::dump()
{
    String dump;
    QTextStream out(&dump);

    if(isBreak())
        out << "Break";
    else if(isLineBreak())
        out << "LineBreak";
    else
        out << _class.typeAsString();
    out << ".";

    out << (_class.flushMode() == GemClass::FlushInherit? "I"
          : _class.flushMode() == GemClass::FlushLeft?    "L"
          : _class.flushMode() == GemClass::FlushRight?   "R" : "C");
    if(!_text.isEmpty()) out << ": `" << _text << "'";

    QStringList spec;
    if(_class.style()) spec << _class.styleAsString();
    if(width()) spec << QString("width=%1").arg(width());
    if(_class.hasFilter()) spec << QString("[%1]").arg(_class.filter());

    if(!spec.isEmpty())
    {
        foreach(QString s, spec)
            out << " " << s;
    }

    return dump;
}
