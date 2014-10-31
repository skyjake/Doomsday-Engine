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
#include "ui/dialogs/logsettingsdialog.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <de/FIFO>
#include <de/App>
#include <de/ChoiceWidget>

#include <de/DialogContentStylist>
#include <de/NotificationAreaWidget>
#include <de/SequentialLayout>
#include <de/SignalAction>
#include <de/ui/ListData>
#include <de/ui/ActionItem>

#include <QTimer>

using namespace de;

static String const VAR_AUTOHIDE = "alert.autoHide";

DENG_GUI_PIMPL(AlertDialog)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DENG2_OBSERVES(Variable, Change)
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
    QTimer hideTimer; ///< Automatically hides the notification.
    ChoiceWidget *autohideTimes;
    DialogContentStylist stylist;

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

        alerts->organizer().audienceForWidgetCreation() += this;
        alerts->organizer().audienceForWidgetUpdate() += this;
        
        // Set up the automatic hide timer.
        QObject::connect(&hideTimer, SIGNAL(timeout()), thisPublic, SLOT(hideNotification()));
        hideTimer.setSingleShot(true);
        App::config(VAR_AUTOHIDE).audienceForChange() += this;
    }

    ~Instance()
    {
        if(!notification->parentWidget())
        {
            GuiWidget::destroy(notification);
        }
        
        App::config(VAR_AUTOHIDE).audienceForChange() -= this;
    }

    NotificationAreaWidget &notifs()
    {
        return ClientWindow::main().notifications();
    }

    int autoHideAfterSeconds() const
    {
        return App::config().geti(VAR_AUTOHIDE, 3 * 60);
    }

    void variableValueChanged(Variable &, Value const &)
    {
        // Update the auto-hide timer.
        if(!autoHideAfterSeconds())
        {
            // Never autohide.
            hideTimer.stop();
        }
        else
        {
            hideTimer.setInterval(autoHideAfterSeconds() * 1000);
            if(!hideTimer.isActive()) hideTimer.start();
        }
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
        label.setAppearanceAnimation(LabelWidget::AppearGrowVertically, 0.5);
        label.setAlignment(ui::AlignBottom);
        label.setImage(style().images().image("alert"));
        label.setOverrideImageSize(style().fonts().font("default").height().value());
        label.setImageAlignment(ui::AlignTop);
        label.setTextAlignment(ui::AlignRight);
        label.margins()
                .setLeft("")
                .setRight("")
                .setBottom("");

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

        // Restart the autohiding timer.
        if(autoHideAfterSeconds() > 0)
        {
            hideTimer.start(autoHideAfterSeconds() * 1000);
        }
    }
    
    void hideNotification()
    {
        notifs().hideChild(*notification);
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
            hideNotification();
            return true; // was hidden
        }
        return false;
    }

    void updateAutohideTimeSelection()
    {
        int const time = autoHideAfterSeconds();
        ui::DataPos pos = autohideTimes->items().findData(time);
        if(pos != ui::Data::InvalidPos)
        {
            autohideTimes->setSelected(pos);
        }
        else
        {
            autohideTimes->setSelected(autohideTimes->items().findData(0));
        }
    }
};

AlertDialog::AlertDialog(String const &/*name*/) : d(new Instance(this))
{
    // The dialog is connected to the notification icon.
    setAnchorAndOpeningDirection(d->notification->rule(), ui::Down);

    buttons() << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default,
                                      tr("Clear All"))
              << new DialogButtonItem(DialogWidget::Action | DialogWidget::Id1,
                                      style().images().image("gear"),
                                      new SignalAction(this, SLOT(showLogFilterSettings())));

    // Auto-hide setting, positioned next to the Gear button.
    // These are not part of the dialog proper, but should be styled like regular
    // dialog contents.
    d->stylist.setContainer(*this);
    ButtonWidget const &gearButton = *buttonWidget(DialogWidget::Id1);

    auto *lab = LabelWidget::newWithText(tr("Hide After:"), this);

    add(d->autohideTimes = new ChoiceWidget);
    d->autohideTimes->items()
            << new ChoiceItem(tr("1 min"),   60)
            << new ChoiceItem(tr("3 mins"),  3 * 60)
            << new ChoiceItem(tr("5 mins"),  5 * 60)
            << new ChoiceItem(tr("10 mins"), 10 * 60)
            << new ChoiceItem(tr("Never"),   0);
    d->updateAutohideTimeSelection();
    
    lab->rule()
        .setInput(Rule::Left, gearButton.rule().right())
        .setMidAnchorY(gearButton.rule().midY());
    
    d->autohideTimes->rule()
        .setInput(Rule::Left, lab->rule().right())
        .setInput(Rule::Top,  lab->rule().top());

    // Tell the dialog about the additional space requirements.
    setMinimumContentWidth(extraButtonsMenu().rule().width() +
                           buttonsMenu().rule().width() +
                           lab->rule().width() +
                           d->autohideTimes->rule().width());

    connect(d->autohideTimes, SIGNAL(selectionChangedByUser(uint)), this, SLOT(autohideTimeChanged()));
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

void AlertDialog::showLogFilterSettings()
{
    LogSettingsDialog *st = new LogSettingsDialog;
    st->setAnchorAndOpeningDirection(buttonWidget(DialogWidget::Id1)->rule(), ui::Left);
    st->setDeleteAfterDismissed(true);
    connect(this, SIGNAL(closed()), st, SLOT(close()));
    st->exec(root());
}

void AlertDialog::hideNotification()
{
    d->hideNotification();
}

void AlertDialog::autohideTimeChanged()
{
    App::config().set(VAR_AUTOHIDE, d->autohideTimes->selectedItem().data().toInt());
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
