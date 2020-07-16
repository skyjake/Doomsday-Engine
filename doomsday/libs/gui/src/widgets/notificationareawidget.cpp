/** @file notificationareawidget.cpp  Notification area.
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

#include "de/notificationareawidget.h"
#include "de/sequentiallayout.h"
#include "de/relaywidget.h"

#include <de/drawable.h>
#include <de/matrix.h>
#include <de/animationrule.h>

#include <de/keymap.h>
#include <de/timer.h>

namespace de {

static constexpr TimeSpan ANIM_SPAN = 500_ms;

DE_GUI_PIMPL(NotificationAreaWidget)
, DE_OBSERVES(Widget, Deletion)
{
    AnimationRule *shift;
    KeyMap<GuiWidget *, RelayWidget *> shown;

    Timer dismissTimer;
    List<GuiWidget *> pendingDismiss;

    Impl(Public *i) : Base(i)
    {
        dismissTimer.setSingleShot(true);
        dismissTimer.setInterval(ANIM_SPAN);
        dismissTimer += [this](){
            root().window().glActivate();
            self().dismiss();
        };

        shift = new AnimationRule(0);
    }

    ~Impl()
    {
#ifdef DE_DEBUG
        for (auto &w : shown)
        {
            DE_ASSERT(w.first->audienceForDeletion().contains(this));
        }
#endif
        releaseRef(shift);
    }

    void updateChildLayout()
    {
        const Rule &gap = rule(RuleBank::UNIT);

        // The children are laid out simply in a row from right to left.
        SequentialLayout layout(self().rule().right(), self().rule().top(), ui::Left);

        bool first = true;
        for (GuiWidget *child : self().childWidgets())
        {
            GuiWidget *w = child->as<RelayWidget>().target();
            DE_ASSERT(w != nullptr);
            if (!first)
            {
                layout << gap;
            }
            first = false;

            layout << *w;
        }

        // Update the total size of the notification area.
        self().rule().setSize(layout.width(), layout.height());
    }

    void show()
    {
        shift->set(0, ANIM_SPAN);
        shift->setStyle(Animation::EaseOut);
        self().show();
    }

    void hide(TimeSpan span = ANIM_SPAN)
    {
        shift->set(self().rule().height() + rule("gap"), span);
        shift->setStyle(Animation::EaseIn);
    }

    void removeChild(GuiWidget &notif)
    {
        DE_ASSERT(shown.contains(&notif));
        auto *relay = shown.take(&notif);
        /*
         * Can't destroy the relay immediately because both the relay and we are
         * observing the notification for deletion and we don't know if the relay
         * will still be notified after this.
         */
        self().remove(*relay);
        GuiWidget::destroyLater(relay);

        if (!self().childCount())
        {
            self().hide();
        }
        updateChildLayout();
    }

    void dismissChild(GuiWidget &notif)
    {
        DE_ASSERT(notif.audienceForDeletion().contains(this));
        notif.audienceForDeletion() -= this;

        removeChild(notif);

        notif.deinitialize();
        notif.setRoot(nullptr);
    }

    void performPendingDismiss()
    {
        dismissTimer.stop();

        // The pending children were already asked to be dismissed.
        for (GuiWidget *w : pendingDismiss)
        {
            dismissChild(*w);
        }
        pendingDismiss.clear();
    }

    void widgetBeingDeleted(Widget &notif)
    {
        GuiWidget *w = static_cast<GuiWidget *>(&notif);
        pendingDismiss.removeAll(w);
        removeChild(*w);
    }
};

NotificationAreaWidget::NotificationAreaWidget(const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{
    // Initially the widget is empty.
    rule().setSize(Const(0), Const(0));
    d->shift->set(style().fonts().font("default").height().valuei() +
                  rule("gap").valuei() * 3);
    hide();
}

void NotificationAreaWidget::useDefaultPlacement(const RuleRectangle &area,
                                                 const Rule &horizontalOffset)
{
    rule().setInput(Rule::Top,   area.top() + rule("gap") - shift())
          .setInput(Rule::Right, area.right() - rule("gap") + horizontalOffset);
}

const Rule &NotificationAreaWidget::shift()
{
    return *d->shift;
}

void NotificationAreaWidget::showChild(GuiWidget &notif)
{
    if (isChildShown(notif))
    {
        // Already in the notification area.
        return;
    }

    // Cancel a pending dismissal.
    d->performPendingDismiss();

    notif.setRoot(&root());
    notif.audienceForDeletion() += d;

    // Set a background for all notifications.
    notif.set(Background(style().colors().colorf("background")));

    auto *relay = new RelayWidget(&notif);
    d->shown.insert(&notif, relay);
    add(relay);
    relay->initialize();
    d->updateChildLayout();
    d->show();
}

void NotificationAreaWidget::hideChild(GuiWidget &notif)
{
    if (!isChildShown(notif))
    {
        // Already in the notification area.
        return;
    }

    if (childCount() > 1)
    {
        // Dismiss immediately, the area itself remains open.
        d->dismissChild(notif);
    }
    else
    {
        // The last one should be deferred until the notification area
        // itself is dismissed.
        d->dismissTimer.start();
        d->pendingDismiss << &notif;
        d->hide();
    }
}

void NotificationAreaWidget::dismiss()
{
    d->performPendingDismiss();
}

bool NotificationAreaWidget::isChildShown(GuiWidget &notif) const
{
    if (d->pendingDismiss.contains(&notif))
    {
        return false;
    }
    return d->shown.contains(&notif);
}

} // namespace de
