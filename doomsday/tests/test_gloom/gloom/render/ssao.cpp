/** @file ssao.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gloom/render/ssao.h"

namespace gloom {

DENG2_PIMPL(SSAO)
{
    Impl(Public *i) : Base(i)
    {}
};

SSAO::SSAO()
    : d(new Impl(this))
{}

void SSAO::glInit(const Context &context)
{
    Render::glInit(context);
}

void SSAO::glDeinit()
{
    Render::glDeinit();
}

void SSAO::render()
{

}

} // namespace gloom
