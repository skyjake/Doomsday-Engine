/** @file render/skybox.h
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

#ifndef GLOOM_SKYBOX_H
#define GLOOM_SKYBOX_H

#include <de/atlastexture.h>
#include <de/matrix.h>

#include "gloom/render/render.h"

namespace gloom {

class SkyBox : public Render
{
public:
    SkyBox();

    void setSize(float scale);

    void glInit(Context &) override;
    void glDeinit() override;
    void render() override;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_SKYBOX_H
