/** @file notificationareawidget.cpp  Notification area.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/NotificationAreaWidget"
#include "de/SequentialLayout"
#include "de/RelayWidget"

#include <de/Drawable>
#include <de/Matrix>
#include <de/ScalarRule>

#include <QMap>
#include <QTimer>

namespace de {

static TimeDelta const ANIM_SPAN = .5;

DENG_GUI_PIMPL(NotificationAreaWidget)
, DENG2_OBSERVES(Widget, Deletion)
{
    ScalarRule *shift;
    QMap<GuiWidget *, RelayWidget *> shown;

    QTimer dismissTimer;
    QList<GuiWidget *> pendingDismiss;

    Instance(Public *i) : Base(i)
    {
        dismissTimer.setSingleShot(true);
        dismissTimer.setInterval(ANIM_SPAN.asMilliSeconds());
        QObject::connect(&dismissTimer, SIGNAL(timeout()), thisPublic, SLOT(dismiss()));

        shift = new ScalarRule(0);
    }

    ~Instance()
    {
        qDebug() << this << "~NotificationArea" << shown.size() << self.childCount();
        foreach(GuiWidget *w, shown.keys())
        {
            qDebug() << "leaving audience of" << w;
            DENG2_ASSERT(w->audienceForDeletion().contains(this));
            w->audienceForDeletion() -= this;
        }
        releaseRef(shift);
    }

    void updateChildLayout()
    {
        Rule const &gap = style().rules().rule("unit");

        // The children are laid out simply in a row from right to left.
        SequentialLayout layout(self.rule().right(), self.rule().top(), ui::Left);

        bool first = true;
        foreach(Widget *child, self.childWidgets())
        {
            GuiWidget *w = child->as<RelayWidget>().target();
            DENG2_ASSERT(w != nullptr);
            if(!first)
            {
                layout << gap;
            }
            first = false;

            layout << *w;
        }

        // Update the total size of the notification area.
        self.rule().setSize(layout.width(), layout.height());
    }

    void show()
    {
        shift->set(0, ANIM_SPAN);
        shift->setStyle(Animation::EaseOut);
        self.show();
    }

    void hide(TimeDelta const &span = ANIM_SPAN)
    {
        shift->set(self.rule().height() + style().rules().rule("gap"), span);
        shift->setStyle(Animation::EaseIn);
    }

    void removeChild(GuiWidget &notif)
    {
        DENG2_ASSERT(shown.contains(&notif));
        auto *relay = shown.take(&notif);
        /*
         * Can't destroy the relay immediately because both the relay and we are
         * observing the notification for deletion and we don't know if the relay
         * will still be notified after this.
         */
        self.remove(*relay);
        GuiWidget::destroyLater(relay);
        
        if(!self.childCount())
        {
            self.hide();
        }
        updateChildLayout();
    }
    
    void dismissChild(GuiWidget &notif)
    {
        DENG2_ASSERT(notif.audienceForDeletion().contains(this));
        notif.audienceForDeletion() -= this;
        
        removeChild(notif);

        notif.deinitialize();
        notif.setRoot(nullptr);
    }

    void performPendingDismiss()
    {
        dismissTimer.stop();

        // The pending children were already asked to be dismissed.
        foreach(GuiWidget *w, pendingDismiss)
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

NotificationAreaWidget::NotificationAreaWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    // Initially the widget is empty.
    rule().setSize(Const(0), Const(0));
    d->shift->set(style().fonts().font("default").height().valuei() +
                  style().rules().rule("gap").valuei() * 3);
    hide();
}

void NotificationAreaWidget::useDefaultPlacement(RuleRectangle const &area)
{
    rule().setInput(Rule::Top,   area.top() + style().rules().rule("gap") - shift())
          .setInput(Rule::Right, area.right() - style().rules().rule("gap"));
}

Rule const &NotificationAreaWidget::shift()
{
    return *d->shift;
}

void NotificationAreaWidget::showChild(GuiWidget &notif)
{
    if(isChildShown(notif))
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
    relay->initialize();
    add(relay);
    d->updateChildLayout();
    d->show();
}

void NotificationAreaWidget::hideChild(GuiWidget &notif)
{
    if(!isChildShown(notif))
    {
        // Already in the notification area.
        return;
    }

    if(childCount() > 1)
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
    if(d->pendingDismiss.contains(&notif))
    {
        return false;
    }
    return d->shown.contains(&notif);
}

} // namespace de
