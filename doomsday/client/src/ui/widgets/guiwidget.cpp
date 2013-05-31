/** @file guiwidget.cpp  Base class for graphical widgets.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "ui/widgets/guiwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "clientapp.h"
#include <de/garbage.h>
#include <de/MouseEvent>

using namespace de;

DENG2_PIMPL(GuiWidget)
{
    RuleRectangle rule;
    Rectanglei savedPos;
    bool inited;
    bool needGeometry;
    Background background;

    Instance(Public *i) : Base(i), inited(false), needGeometry(true)
    {}

    ~Instance()
    {
        // Deinitialize now if it hasn't been done already.
        self.deinitialize();
    }
};

GuiWidget::GuiWidget(String const &name) : Widget(name), d(new Instance(this))
{}

GuiRootWidget &GuiWidget::root()
{
    return static_cast<GuiRootWidget &>(Widget::root());
}

Style const &GuiWidget::style()
{
    return ClientApp::windowSystem().style();
}

RuleRectangle &GuiWidget::rule()
{
    return d->rule;
}

RuleRectangle const &GuiWidget::rule() const
{
    return d->rule;
}

static void deleteGuiWidget(void *ptr)
{
    delete reinterpret_cast<GuiWidget *>(ptr);
}

void GuiWidget::deleteLater()
{
    Garbage_TrashInstance(this, deleteGuiWidget);
}

void GuiWidget::set(Background const &bg)
{
    d->background = bg;
}

GuiWidget::Background const &GuiWidget::background() const
{
    return d->background;
}

void GuiWidget::initialize()
{
    if(d->inited) return;

    try
    {
        d->inited = true;
        glInit();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Error when initializing widget '%s':\n")
                << name() << er.asText();
    }
}

void GuiWidget::deinitialize()
{
    if(!d->inited) return;

    try
    {
        d->inited = false;
        glDeinit();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Error when deinitializing widget '%s':\n")
                << name() << er.asText();
    }
}

void GuiWidget::update()
{
    if(!d->inited)
    {
        initialize();
    }
}

bool GuiWidget::hitTest(Vector2i const &pos) const
{
    if(behavior().testFlag(Unhittable))
    {
        // Can never be hit by anything.
        return false;
    }
    return rule().recti().contains(pos);
}

GuiWidget::MouseClickStatus GuiWidget::handleMouseClick(Event const &event)
{
    if(event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.button() != MouseEvent::Left)
        {
            return MouseClickUnrelated;
        }

        if(mouse.state() == MouseEvent::Pressed && hitTest(mouse.pos()))
        {
            root().routeMouse(this);
            return MouseClickStarted;
        }

        if(mouse.state() == MouseEvent::Released && root().isEventRouted(event.type(), this))
        {
            root().routeMouse(0);
            if(hitTest(mouse.pos()))
            {
                return MouseClickFinished;
            }
            return MouseClickAborted;
        }
    }
    return MouseClickUnrelated;
}

void GuiWidget::glInit()
{}

void GuiWidget::glDeinit()
{}

void GuiWidget::requestGeometry(bool yes)
{
    d->needGeometry = yes;
}

bool GuiWidget::geometryRequested() const
{
    return d->needGeometry;
}

void GuiWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    // Is there a solid fill?
    if(d->background.solidFill.w > 0)
    {
        verts.makeQuad(rule().recti(),
                       d->background.solidFill,
                       root().atlas().imageRectf(root().solidWhitePixel()).middle());
    }

    switch(d->background.type)
    {
    case Background::GradientFrame:
        verts.makeFlexibleFrame(rule().recti(),
                                d->background.thickness,
                                d->background.color,
                                root().atlas().imageRectf(root().gradientFrame()));
        break;

    case Background::None:
        break;
    }
}

bool GuiWidget::hasChangedPlace(Rectanglei &currentPlace)
{
    currentPlace = rule().recti();
    bool changed = (d->savedPos != currentPlace);
    d->savedPos = currentPlace;
    return changed;
}
