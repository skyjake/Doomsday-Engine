/** @file indirectrule.cpp  Indirect rule.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/IndirectRule"

namespace de {

IndirectRule::IndirectRule() : _source(0)
{}

IndirectRule::~IndirectRule()
{
    independentOf(_source);
}

void IndirectRule::setSource(Rule const &rule)
{
    unsetSource();
    dependsOn(_source = &rule);

    invalidate();
}

void IndirectRule::unsetSource()
{
    independentOf(_source);
    _source = 0;

    invalidate();
}

void IndirectRule::update()
{
    setValue(_source? _source->value() : 0);
}

Rule const &IndirectRule::source() const
{
    DENG2_ASSERT(_source != 0);
    return *_source;
}

String IndirectRule::description() const
{
    if(_source)
    {
        return source().description();
    }
    else
    {
        return String("(null)");
    }
}

} // namespace de
