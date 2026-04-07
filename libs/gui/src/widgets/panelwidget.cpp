/** @file panelwidget.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/panelwidget.h"
#include "de/guirootwidget.h"

#include <de/animationrule.h>
#include <de/drawable.h>
#include <de/garbage.h>
#include <de/logbuffer.h>
#include <de/mouseevent.h>
#include <de/timer.h>
#include <de/math.h>

namespace de {

static constexpr TimeSpan OPENING_ANIM_SPAN           = 300_ms;
static constexpr TimeSpan OPENING_ANIM_SPAN_EASED_OUT = 180_ms;
static constexpr TimeSpan CLOSING_ANIM_SPAN           = 220_ms;

DE_GUI_PIMPL(PanelWidget)
, DE_OBSERVES(Asset, StateChange)
{
    bool waitForContentReady = true;
    bool eatMouseEvents      = true;
    bool opened              = false;

    ui::Direction  dir                 = ui::Down;
    ui::SizePolicy secondaryPolicy     = ui::Expand;
    GuiWidget *    content             = nullptr;
    AnimationRule *openingRule;
    AnimationStyle openingStyle = EasedOut;
    Timer dismissTimer;

    std::unique_ptr<AssetGroup> pendingShow;

    GuiVertexBuilder verts;

    Impl(Public *i) : Base(i)
    {
        openingRule = new AnimationRule(0);
        openingRule->setBehavior(AnimationRule::RestartWhenTargetChanges);

        dismissTimer.setSingleShot(true);
        dismissTimer += [this](){ self().dismiss(); };
    }

    ~Impl()
    {
        releaseRef(openingRule);
    }

    void glInit() {}

    void glDeinit()
    {
        verts.clear();
    }

    bool isVerticalAnimation() const
    {
        return isVertical(dir) || dir == ui::NoDirection;
    }

    void updateLayout()
    {
        // Widget's size depends on the opening animation.
        if (isVerticalAnimation())
        {
            self().rule().setInput(Rule::Height, *openingRule);
            if (secondaryPolicy == ui::Expand)
            {
                self().rule().setInput(Rule::Width, content ? content->rule().width() : Const(0));
            }
        }
        else
        {
            self().rule().setInput(Rule::Width, *openingRule);
            if (secondaryPolicy == ui::Expand)
            {
                self().rule().setInput(Rule::Height, content ? content->rule().height() : Const(0));
            }
        }
    }

    void updateGeometry()
    {
        Rectanglei pos;
        if (self().hasChangedPlace(pos) || self().geometryRequested())
        {
            self().requestGeometry(false);
            verts.clear();
            self().glMakeGeometry(verts);
        }
    }

    void startOpeningAnimation(const TimeSpan& span)
    {
        if (isVerticalAnimation())
        {
            openingRule->set(content->rule().height(), span);
        }
        else
        {
            openingRule->set(content->rule().width(), span);
        }

        switch (openingStyle)
        {
        case Bouncy:   openingRule->setStyle(Animation::Bounce, 12); break;
        case EasedOut: openingRule->setStyle(Animation::EaseOutSofter); break;
        case Smooth:   openingRule->setStyle(Animation::EaseBoth); break;
        }
    }

    void close(const TimeSpan& delay)
    {
        if (!opened) return;

        opened = false;

        self().setBehavior(DisableEventDispatchToChildren);

        // Begin the closing animation.
        openingRule->set(0, CLOSING_ANIM_SPAN + delay, delay);
        switch (openingStyle)
        {
        case Bouncy:
        case EasedOut:
            openingRule->setStyle(Animation::EaseIn);
            break;
        case Smooth:
            openingRule->setStyle(Animation::EaseBoth);
            break;
        }

        self().panelClosing();

        DE_NOTIFY_PUBLIC(Close, i)
        {
            i->panelBeingClosed(self());
        }

        dismissTimer.start();
        dismissTimer.setInterval(CLOSING_ANIM_SPAN + delay);
    }

    void waitForAssetsInContent()
    {
        if (!waitForContentReady) return;

        LOG_AS("PanelWidget");

        pendingShow.reset(new AssetGroup);

        LOGDEV_XVERBOSE("Checking for assets that need waiting for...", "");
        DE_ASSERT(content);
        content->collectUnreadyAssets(*pendingShow);

        if (pendingShow->isEmpty())
        {
            // Nothing to wait for, actually.
            pendingShow.reset();
            return;
        }

        LOGDEV_VERBOSE("Waiting for %i assets to become ready") << pendingShow->size();

        pendingShow->audienceForStateChange() += this;
        openingRule->pause();
    }

    void assetStateChanged(Asset &)
    {
        LOG_AS("PanelWidget");

        // All of the assets in the pending show group are now ready, let's open!
        if (pendingShow && pendingShow->isReady())
        {
            LOGDEV_XVERBOSE("All assets ready, resuming animation", "");

            openingRule->resume();

            // We can't delete the AssetGroup right now because we are in the middle
            // of an audience notification from it.
            pendingShow->audienceForStateChange() -= this;
            trash(pendingShow.release());
        }
    }

    DE_PIMPL_AUDIENCES(AboutToOpen, Open, Close, Dismiss)
};

DE_AUDIENCE_METHODS(PanelWidget, AboutToOpen, Open, Close, Dismiss)

PanelWidget::PanelWidget(const String &name) : GuiWidget(name), d(new Impl(this))
{
    setBehavior(ChildHitClipping);
    setBehavior(ChildVisibilityClipping);
    d->updateLayout(); // initial, empty layout
    hide();
}

void PanelWidget::setWaitForContentReady(bool yes)
{
    d->waitForContentReady = yes;
}

void PanelWidget::setAnimationStyle(AnimationStyle style)
{
    d->openingStyle = style;
}

void PanelWidget::setEatMouseEvents(bool yes)
{
    d->eatMouseEvents = yes;
}

void PanelWidget::setContent(GuiWidget *content)
{
    if (d->content)
    {
        destroy(takeContent());
    }

    d->content = content;
    add(content); // ownership taken

    // Place the content inside the panel.
    content->rule()
            .setInput(Rule::Left, rule().left())
            .setInput(Rule::Top,  rule().top());
}

GuiWidget &PanelWidget::content() const
{
    DE_ASSERT(d->content != nullptr);
    return *d->content;
}

GuiWidget *PanelWidget::takeContent()
{
    GuiWidget *w = d->content;
    if (!w) { return nullptr; }

    d->content = nullptr;

    w->rule().clearInput(Rule::Left);
    w->rule().clearInput(Rule::Top);

    if (d->secondaryPolicy == ui::Expand)
    {
        rule().clearInput(Rule::Width);
        rule().clearInput(Rule::Height);
    }

    remove(*w);
    return w;
}

void PanelWidget::setOpeningDirection(ui::Direction dir)
{
    d->dir = dir;
}

void PanelWidget::setSizePolicy(ui::SizePolicy policy)
{
    d->secondaryPolicy = policy;
}

ui::Direction PanelWidget::openingDirection() const
{
    return d->dir;
}

bool PanelWidget::isOpen() const
{
    return d->opened;
}

bool PanelWidget::isOpeningOrClosing() const
{
    return !d->openingRule->animation().done();
}

void PanelWidget::close(TimeSpan delayBeforeClosing)
{
    d->close(std::move(delayBeforeClosing));
}

void PanelWidget::viewResized()
{
    GuiWidget::viewResized();
    requestGeometry();
}

void PanelWidget::update()
{
    GuiWidget::update();
}

bool PanelWidget::handleEvent(const Event &event)
{
    if (d->eatMouseEvents && event.type() == Event::MouseButton)
    {
        const MouseEvent &mouse = event.as<MouseEvent>();

        // Eat buttons that land on the panel.
        if (hitTest(mouse.pos()))
        {
            return true;
        }
    }
    return GuiWidget::handleEvent(event);
}

void PanelWidget::open()
{
    if (d->opened) return;

    DE_NOTIFY(AboutToOpen, i) { i->panelAboutToOpen(*this); }

    d->dismissTimer.stop();

    unsetBehavior(DisableEventDispatchToChildren);
    show();

    preparePanelForOpening();

    // Start the opening animation.
    d->startOpeningAnimation(d->openingStyle == EasedOut ? OPENING_ANIM_SPAN_EASED_OUT
                                                         : OPENING_ANIM_SPAN);

    d->opened = true;

    DE_NOTIFY(Open, i) { i->panelOpened(*this); }

    // The animation might have to be paused until all assets are prepared.
    d->waitForAssetsInContent();
}

void PanelWidget::close()
{
    d->close(0.2);
}

void PanelWidget::openOrClose()
{
    if (isOpen() || isOpeningOrClosing())
    {
        close();
    }
    else
    {
        open();
    }
}

void PanelWidget::dismiss()
{
    if (isHidden()) return;

    root().window().glActivate();

    hide();
    d->opened = false;
    d->dismissTimer.stop();

    panelDismissed();

    DE_NOTIFY(Dismiss, i) { i->panelDismissed(*this); }
}

void PanelWidget::drawContent()
{
    d->updateGeometry();

    if (d->verts)
    {
        auto &painter = root().painter();
        painter.setColor(Vec4f(1));
        painter.drawTriangleStrip(d->verts);
    }
}

void PanelWidget::glMakeGeometry(GuiVertexBuilder &verts)
{
    GuiWidget::glMakeGeometry(verts);
}

void PanelWidget::glInit()
{
    d->glInit();
}

void PanelWidget::glDeinit()
{
    d->glDeinit();
}

void PanelWidget::preparePanelForOpening()
{
    d->updateLayout();
}

void PanelWidget::panelClosing()
{
    // to be overridden
}

void PanelWidget::panelDismissed()
{
    // nothing to do
}

} // namespace de
