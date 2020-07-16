/** @file indirectrule.cpp  Indirect rule.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/indirectrule.h"

namespace de {

IndirectRule::IndirectRule() : _source(nullptr)
{}

IndirectRule::~IndirectRule()
{
    independentOf(_source);
}

void IndirectRule::setSource(const Rule &rule)
{
    unsetSource();
    dependsOn(_source = &rule);
    invalidate();
}

void IndirectRule::unsetSource()
{
    independentOf(_source);
    _source = nullptr;
    invalidate();
}

bool IndirectRule::hasSource() const
{
    return _source != nullptr;
}

void IndirectRule::update()
{
    setValue(_source ? _source->value() : 0.f);
}

const Rule &IndirectRule::source() const
{
    DE_ASSERT(_source);
    return *_source;
}

String IndirectRule::description() const
{
    if (_source)
    {
        return source().description();
    }
    else
    {
        return String("[0]");
    }
}

} // namespace de
