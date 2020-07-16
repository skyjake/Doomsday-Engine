/** @file gloomwidget.cpp
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

#include "gloomwidget.h"

#include <gloom/audio/audiosystem.h>
#include <gloom/world.h>
#include <gloom/world/user.h>
#include <de/drawable.h>
#include <de/glbuffer.h>
#include <de/keyevent.h>
#include <de/matrix.h>
#include <de/vrconfig.h>

using namespace de;

namespace gloom {

DE_GUI_PIMPL(GloomWidget)
{
    Mat4f            modelView;
    SafePtr<IWorld>  world;
    Time             previousUpdateAt;
    User             user;
    User::InputState inputs;
    bool             mouseLook = false;

    Impl(Public *i) : Base(i)
    {}

    void glInit()
    {
        if (world) { world->glInit(); }
        updateModelView();
        AudioSystem::get().setListener(&self());
    }

    void updateModelView()
    {
        modelView = Mat4f::rotate(user.pitch(), Vec3f(1, 0, 0)) *
                    Mat4f::rotate(user.yaw(),   Vec3f(0, 1, 0)) *
                    Mat4f::translate(-user.position());
    }

    Mat4f viewMatrix() const
    {
        return modelView;
    }

    void glDeinit()
    {
        if (world) world->glDeinit();
    }
};

GloomWidget::GloomWidget()
    : GuiWidget("gloomwidget")
    , d(new Impl(this))
{}

IWorld *GloomWidget::world() const
{
    return d->world;
}

User &GloomWidget::user()
{
    return d->user;
}

void GloomWidget::setCameraPosition(const Vec3f &pos)
{
    d->user.setPosition(pos);
}

void GloomWidget::setCameraYaw(float yaw)
{
    d->user.setYaw(yaw);
}

void GloomWidget::setWorld(IWorld *world)
{
    const IWorld *oldWorld = d->world;

    if (d->world)
    {
        if (isInitialized())
        {
            d->world->glDeinit();
        }
        d->world->setLocalUser(nullptr);
        d->user.setWorld(nullptr);
    }

    d->world = world;
    DE_NOTIFY_VAR(Change, i) { i->currentWorldChanged(oldWorld, world); }

    if (d->world)
    {
        d->world->setLocalUser(&d->user);
        if (isInitialized())
        {
            try
            {
                d->world->glInit();
            }
            catch (const Error &er)
            {
                LOG_ERROR("Failed to initialize world for drawing: %s") << er.asText();
            }
        }
    }
}

void GloomWidget::update()
{
    GuiWidget::update();

    // How much time has passed?
    const TimeSpan elapsed = d->previousUpdateAt.since();
    d->previousUpdateAt = Time();

    if (d->world)
    {
        d->world->update(elapsed);
    }
    d->user.setInputState(d->inputs);
    d->user.update(elapsed);

    d->updateModelView();
}

void GloomWidget::drawContent()
{
    if (d->world)
    {
        // Any buffered draws should be done before rendering the world.
        auto &painter = root().painter();
        painter.flush();
        GLState::push().setNormalizedScissor(painter.normalizedScissor());

        d->world->render(*this);

        GLState::pop();
    }
}

bool GloomWidget::handleEvent(const Event &event)
{
    if (event.isKey())
    {
        const KeyEvent &key = event.as<KeyEvent>();

        // Check for some key commands.
        if (key.isKeyDown())
        {
            if (key.ddKey() >= '0' && key.ddKey() <= '9')
            {
                d->world->as<World>().setDebugMode(key.ddKey() - '0');
                return true;
            }
        }

        User::InputBit bit = User::Inert;
        switch (key.ddKey())
        {
        case 'q':
        case DDKEY_LEFTARROW:  bit = User::TurnLeft;  break;
        case 'e':
        case DDKEY_RIGHTARROW: bit = User::TurnRight; break;
        case 'w':
        case DDKEY_UPARROW:    bit = User::Forward;   break;
        case 's':
        case DDKEY_DOWNARROW:  bit = User::Backward;  break;
        case 'a':              bit = User::StepLeft;  break;
        case 'd':              bit = User::StepRight; break;
        case ' ':              bit = User::Jump;      break;
        case DDKEY_LSHIFT:     bit = User::Shift;     break;
        default:               break;
        }

        if (bit)
        {
            if (key.state() != KeyEvent::Released)
                d->inputs |= bit;
            else
                d->inputs &= ~bit;
        }
    }

    if (event.isMouse())
    {
        const MouseEvent &mouse = event.as<MouseEvent>();

        if (mouse.type() == Event::MouseWheel)
        {
            d->user.turn(Vec2f(mouse.wheel()) / 1.f);
            return true;
        }
        if (mouse.type() == Event::MouseMotion && d->mouseLook)
        {
            d->user.turn(Vec2f(mouse.pos()) / 7.f);
            return true;
        }
        if (mouse.type() == Event::MouseButton)
        {
            switch (handleMouseClick(event, MouseEvent::Left))
            {
                case MouseClickFinished:
                    d->mouseLook = !d->mouseLook;
                    root().window().eventHandler().trapMouse(d->mouseLook);
                    break;

                default:
                    break;
            }
            return true;
        }
    }

    return GuiWidget::handleEvent(event);
}

Vec3f GloomWidget::cameraPosition() const
{
    return d->user.position();
}

Vec3f GloomWidget::cameraFront() const
{
    Vec4f v = d->viewMatrix().inverse() * Vec4f(0, 0, -1, 0);
    return v.normalize();
}

Vec3f GloomWidget::cameraUp() const
{
    Vec3f v = d->viewMatrix().inverse() * Vec4f(0, 1, 0, 0);
    return v.normalize();
}

Mat4f GloomWidget::cameraProjection() const
{
    const auto size = rule().size();
    return Mat4f::perspective(80, size.x / size.y, 0.1f, 1500.f);
}

Mat4f GloomWidget::cameraModelView() const
{
    return d->viewMatrix();
}

void GloomWidget::glInit()
{
    GuiWidget::glInit();
    d->glInit();
}

void GloomWidget::glDeinit()
{
    GuiWidget::glDeinit();
    d->glDeinit();
}

} // namespace gloom
