/** @file alertdialog.cpp  Dialog for listing recent alerts.
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

#include "ui/dialogs/alertdialog.h"
#include "ui/widgets/notificationwidget.h"
#include "ui/clientwindow.h"
#include "ui/ListData"
#include "ui/ActionItem"
#include "clientapp.h"
#include "SignalAction"

#include <de/FIFO>
#include <de/App>

using namespace de;

DENG_GUI_PIMPL(AlertDialog)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
{
    /// Data model item representing an alert in the list.
    class AlertItem : public ui::ActionItem
    {
    public:
        AlertItem(String const &msg, Level level)
            : ui::ActionItem(ShownAsLabel, msg)
            , _level(level)
        {}

        Level level() const { return _level; }

    private:
        Level _level;
    };

    /// Rich font styling for alert labels.
    struct TextStyling : public Font::RichFormat::IStyle
    {
        Color richStyleColor(int /*index*/) const
        {
            return Color(255, 255, 255, 255); // not used by LabelWidget
        }

        void richStyleFormat(int contentStyle, float &sizeFactor,
                             Font::RichFormat::Weight &fontWeight,
                             Font::RichFormat::Style &fontStyle,
                             int &colorIndex) const
        {
            ClientApp::windowSystem().style()
                    .richStyleFormat(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);

            if(contentStyle == Font::RichFormat::MajorStyle ||
               contentStyle == Font::RichFormat::MajorMetaStyle)
            {
                // Keep the major style normal-weight.
                fontWeight = Font::RichFormat::Normal;
            }
        }
    };

    ButtonWidget *notification;
    MenuWidget *alerts;
    bool clearOnDismiss;
    TextStyling styling;

    dsize maxCount;
    typedef FIFO<AlertItem> Pending;
    Pending pending;

    Instance(Public *i)
        : Base(i)
        , clearOnDismiss(false)
        , maxCount(100)
    {
        notification = new ButtonWidget;
        notification->setSizePolicy(ui::Expand, ui::Expand);
        notification->setImage(style().images().image("alert"));
        notification->setOverrideImageSize(style().fonts().font("default").height().value());
        notification->setAction(new SignalAction(thisPublic, SLOT(showListOfAlerts())));

        // The menu expands with all the alerts, and the dialog's scroll area allows
        // browsing it up and down.
        ScrollAreaWidget &area = self.area();
        alerts = new MenuWidget;
        alerts->enableScrolling(false);
        alerts->setGridSize(1, ui::Expand, 0, ui::Expand);
        alerts->rule().setLeftTop(area.contentRule().left(), area.contentRule().top());
        area.setContentSize(alerts->rule().width(), alerts->rule().height());
        area.add(alerts);

        area.enableIndicatorDraw(true);

        alerts->organizer().audienceForWidgetCreation += this;
        alerts->organizer().audienceForWidgetUpdate += this;
    }

    NotificationWidget &notifs()
    {
        return ClientWindow::main().notifications();
    }

    void queueAlert(String const &msg, Level level)
    {
        pending.put(new AlertItem(msg, level));
    }

    bool addPendingAlerts()
    {
        bool changed = false;
        while(AlertItem *alert = pending.take())
        {
            add(alert);
            changed = true;
        }
        // Remove excess alerts.
        while(alerts->items().size() > maxCount)
        {
            alerts->items().remove(alerts->items().size() - 1);
            changed = true;
        }
        return changed;
    }

    void add(AlertItem *alert)
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        // If we already have this, don't re-add.
        for(ui::Data::Pos i = 0; i < alerts->items().size(); ++i)
        {
            if(!alerts->items().at(i).label().compareWithoutCase(alert->label()))
                return;
        }

        alerts->items().insert(0, alert);
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        // Background is provided by the popup.
        widget.set(Background());

        LabelWidget &label = widget.as<LabelWidget>();

        // Each alert has an icon identifying the originating subsystem and the level
        // of the alert.
        label.setBehavior(ContentClipping); // allow clip-culling
        label.setTextStyle(&styling);
        label.setMaximumTextWidth(style().rules().rule("alerts.width").valuei());
        label.setSizePolicy(ui::Expand, ui::Expand);
        label.setImage(style().images().image("alert"));
        label.setOverrideImageSize(style().fonts().font("default").height().value());
        label.setImageAlignment(ui::AlignTop);
        label.setTextAlignment(ui::AlignRight);
        label.margins().setLeft("");
        label.margins().setRight("");
        label.margins().setBottom("");

        AlertItem const &alert = item.as<AlertItem>();
        switch(alert.level())
        {
        case Minor:
            label.setImageColor(Vector4f(style().colors().colorf("text"), .5f));
            break;

        case Normal:
            label.setImageColor(style().colors().colorf("text"));
            break;

        case Major:
            label.setTextStyle(0); // use default styling with bold weights
            label.setImageColor(style().colors().colorf("accent"));
            break;
        }
    }

    void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        DENG2_UNUSED(widget);
        DENG2_UNUSED(item);
    }

    void showNotification()
    {
        // Change color to indicate new alerts.
        notification->setImageColor(style().colors().colorf("accent"));

        notifs().showOrHide(notification, true);
    }

    /**
     * @returns @c true, if the notification was hidden due to the alerts list being
     * empty.
     */
    bool hideIfEmpty()
    {
        if(alerts->items().isEmpty())
        {
            // No alerts to show.
            notifs().hideChild(*notification);
            return true; // was hidden
        }
        return false;
    }
};

AlertDialog::AlertDialog(String const &name) : d(new Instance(this))
{
    // The dialog is connected to the notification icon.
    setAnchorAndOpeningDirection(d->notification->rule(), ui::Down);

    buttons() << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default,
                                      tr("Clear All"));
}

void AlertDialog::newAlert(String const &message, Level level)
{
    d->queueAlert(message, level);
}

void AlertDialog::update()
{
    DialogWidget::update();

    if(d->addPendingAlerts())
    {
        d->showNotification();
    }
}

void AlertDialog::showListOfAlerts()
{
    if(isOpen() || d->hideIfEmpty()) return;

    // Restore the normal color.
    d->notification->setImageColor(style().colors().colorf("text"));

    area().scrollToTop(0);
    open();
}

void AlertDialog::finish(int result)
{
    DialogWidget::finish(result);

    // The alerts will be cleared if the dialog is accepted.
    d->clearOnDismiss = (result != 0);
}

void AlertDialog::panelDismissed()
{
    if(d->clearOnDismiss)
    {
        d->clearOnDismiss = false;
        d->alerts->items().clear();
        d->hideIfEmpty();
    }

    DialogWidget::panelDismissed();
}
