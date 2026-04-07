/** @file gloomwidget.h
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

#ifndef GLOOMWIDGET_H
#define GLOOMWIDGET_H

#include <de/guiwidget.h>
#include "gloom/render/icamera.h"

namespace gloom {

class IWorld;
class User;

class GloomWidget
    : public de::GuiWidget
    , public ICamera
{
public:
    DE_DEFINE_AUDIENCE(Change, void currentWorldChanged(const IWorld *old, IWorld *current))

public:
    GloomWidget();

    IWorld *world() const;
    User & user();

    void setCameraPosition(const de::Vec3f &pos);
    void setCameraYaw(float yaw);

    // Events.
    void update();
    void drawContent();
    bool handleEvent(const de::Event &event);

    // Implements ICamera.
    de::Vec3f cameraPosition() const;
    de::Vec3f cameraFront() const;
    de::Vec3f cameraUp() const;
    de::Mat4f cameraModelView() const;
    de::Mat4f cameraProjection() const;

public:
    void setWorld(IWorld *world);

protected:
    void glInit();
    void glDeinit();

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOMWIDGET_H
