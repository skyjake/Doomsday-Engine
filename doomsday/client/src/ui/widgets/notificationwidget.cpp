/** @file notificationwidget.cpp  Notification area.
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

#include "ui/widgets/notificationwidget.h"
#include "SequentialLayout"

#include <de/Drawable>
#include <de/Matrix>
#include <de/ScalarRule>

#include <QMap>
#include <QTimer>

using namespace de;

static TimeDelta const ANIM_SPAN = .5;

DENG_GUI_PIMPL(NotificationWidget),
DENG2_OBSERVES(Widget, ChildAddition),
DENG2_OBSERVES(Widget, ChildRemoval)
{
    ScalarRule *shift;

    typedef QMap<GuiWidget *, Widget *> OldParents;
    OldParents oldParents;
    QTimer dismissTimer;
    QList<GuiWidget *> pendingDismiss;

    // GL objects:
    typedef DefaultVertexBuf VertexBuf;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        self.audienceForChildAddition += this;
        self.audienceForChildRemoval += this;

        dismissTimer.setSingleShot(true);
        dismissTimer.setInterval(ANIM_SPAN.asMilliSeconds());
        QObject::connect(&dismissTimer, SIGNAL(timeout()), thisPublic, SLOT(dismiss()));

        shift = new ScalarRule(0);
        updateStyle();
    }

    ~Instance()
    {
        releaseRef(shift);
    }

    void updateStyle()
    {
        //self.
        //self.set(Background(self.style().colors().colorf("background")));
    }

    void glInit()
    {
        drawable.addBuffer(new VertexBuf);

        shaders().build(drawable.program(), "generic.color_ucolor")
                << uMvpMatrix << uColor;
    }

    void glDeinit()
    {
        drawable.clear();
    }

    void updateGeometry()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || self.geometryRequested())
        {
            self.requestGeometry(false);

            VertexBuf::Builder verts;
            self.glMakeGeometry(verts);
            drawable.buffer<VertexBuf>().setVertices(gl::TriangleStrip, verts, gl::Static);
        }
    }

    void updateChildLayout()
    {
        Rule const &gap = style().rules().rule("unit");

        // The children are laid out simply in a row from right to left.
        SequentialLayout layout(self.rule().right(), self.rule().top(), ui::Left);

        bool first = true;
        foreach(Widget *child, self.childWidgets())
        {
            GuiWidget &w = child->as<GuiWidget>();
            if(!first)
            {
                layout << gap;
            }
            first = false;

            layout << w;
        }

        // Update the total size of the notification area.
        self.rule().setSize(layout.width(), layout.height());
    }

    void show()
    {
        //self.setOpacity(1, ANIM_SPAN);
        shift->set(0, ANIM_SPAN);
        shift->setStyle(Animation::EaseOut);
    }

    void hide(TimeDelta const &span = ANIM_SPAN)
    {
        //self.setOpacity(0, span);
        shift->set(self.rule().height() + style().rules().rule("gap"), span);
        shift->setStyle(Animation::EaseIn);
    }

    void dismissChild(GuiWidget &notif)
    {
        notif.hide();
        self.remove(notif);

        if(oldParents.contains(&notif))
        {
            oldParents[&notif]->add(&notif);
            oldParents.remove(&notif);
        }
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

    void widgetChildAdded(Widget &child)
    {
        // Set a background for all notifications.
        child.as<GuiWidget>().set(Background(style().colors().colorf("background")));

        updateChildLayout();
        self.show();
    }

    void widgetChildRemoved(Widget &)
    {
        updateChildLayout();
        if(!self.childCount())
        {
            self.hide();
        }
    }
};

NotificationWidget::NotificationWidget(String const &name) : d(new Instance(this))
{
    // Initially the widget is empty.
    rule().setSize(Const(0), Const(0));
    d->shift->set(style().fonts().font("default").height().valuei() +
                  style().rules().rule("gap").valuei() * 3);
    hide();
}

Rule const &NotificationWidget::shift()
{
    return *d->shift;
}

void NotificationWidget::showChild(GuiWidget *notif)
{
    DENG2_ASSERT(notif != 0);

    if(isChildShown(*notif))
    {
        // Already in the notification area.
        return;
    }

    // Cancel a pending dismissal.
    d->performPendingDismiss();

    if(notif->parentWidget())
    {
        d->oldParents.insert(notif, notif->parentWidget());
        /// @todo Should observe if the old parent is destroyed.
    }
    add(notif);
    notif->show();
    d->show();
}

void NotificationWidget::hideChild(GuiWidget &notif)
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

void NotificationWidget::dismiss()
{
    d->performPendingDismiss();
}

bool NotificationWidget::isChildShown(GuiWidget &notif) const
{
    if(d->pendingDismiss.contains(&notif))
    {
        return false;
    }
    return notif.parentWidget() == this;
}

void NotificationWidget::viewResized()
{
    GuiWidget::viewResized();

    d->uMvpMatrix = root().projMatrix2D();
}

void NotificationWidget::drawContent()
{
    d->updateGeometry();

    d->uColor = Vector4f(1, 1, 1, visibleOpacity());
    d->drawable.draw();
}

void NotificationWidget::glInit()
{
    d->glInit();
}

void NotificationWidget::glDeinit()
{
    d->glDeinit();
}
