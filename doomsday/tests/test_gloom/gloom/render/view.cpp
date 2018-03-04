/** @file view.cpp
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

#include "gloom/render/view.h"

using namespace de;

namespace gloom {

void View::setCamera(const ICamera &camera)
{
    this->camera = &camera;

    uCameraPos         = Vec4f(camera.cameraPosition(), 1.0f);
    uCameraMvpMatrix   = camera.cameraModelViewProjection();
    uModelViewMatrix   = camera.cameraModelView();
    uWorldToViewRotate = camera.cameraModelView().submatrix(0, 0);
    uViewToWorldRotate = uWorldToViewRotate.toMat3f().inverse();
    uProjMatrix        = camera.cameraProjection();
    uInverseProjMatrix = camera.cameraProjection().inverse();
}

} // namespace gloom
