/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/DelegateRule"

namespace de {

DelegateRule::DelegateRule(ISource &source, int delegateId)
    : ConstantRule(), _source(&source), _delegateId(delegateId)
{}

DelegateRule::~DelegateRule()
{}

void DelegateRule::update()
{
    // Value cannot be determined without a source.
    if(_source)
    {
        // During this, the source is expected to call ConstantRule::set() on us.
        _source->delegateUpdate(_delegateId);
    }

    ConstantRule::update();

    // It must be valid now, after the update.
    DENG2_ASSERT(isValid());
}

void DelegateRule::setSource(DelegateRule::ISource *source)
{
    _source = source;
}

void DelegateRule::invalidate()
{
    if(isValid())
    {
        Rule::invalidate();

        if(_source)
        {
            _source->delegateInvalidation(_delegateId);
        }
    }
}

} // namespace de
