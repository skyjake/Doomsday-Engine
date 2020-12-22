/** @file alertdialog.cpp  Dialog for listing recent alerts.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/app.h>
#include <de/choicewidget.h>
#include <de/config.h>
#include <de/dialogcontentstylist.h>
#include <de/fifo.h>
#include <de/notificationareawidget.h>
#include <de/sequentiallayout.h>
#include <de/timer.h>
#include <de/ui/actionitem.h>
#include <de/ui/listdata.h>
#include <de/windowsystem.h>

using namespace de;

DE_STATIC_STRING(VAR_AUTOHIDE, "alert.autoHide");

DE_GUI_PIMPL(AlertDialog)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DE_OBSERVES(Variable, Change)
{
    /// Data model item representing an alert in the list.
    class AlertItem : public ui::ActionItem
    {
    public:
        AlertItem(const String &msg, Level level)
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
            Style::get().richStyleFormat(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);

            if (contentStyle == Font::RichFormat::MajorStyle ||
               contentStyle == Font::RichFormat::MajorMetaStyle)
            {
                // Keep the major style normal-weight.
                fontWeight = Font::RichFormat::Normal;
            }
        }
    };

    UniqueWidgetPtr<PopupButtonWidget> notification;
    MenuWidget *alerts;
    bool clearOnDismiss;
    TextStyling styling;
    Timer hideTimer; ///< Automatically hides the notification.
    ChoiceWidget *autohideTimes;
    DialogContentStylist stylist;

    dsize maxCount;
    typedef FIFO<AlertItem> Pending;
    Pending pending;

    Impl(Public *i)
        : Base(i)
        , clearOnDismiss(false)
        , maxCount(100)
    {
        notification.reset(new PopupButtonWidget);
        notification->setBehavior(Focusable, false);
        notification->setSizePolicy(ui::Expand, ui::Expand);
        notification->setImage(style().images().image("alert"));
        notification->setOverrideImageSize(style().fonts().font("default").height());
        notification->setOpener([this] (PopupWidget *) {
            self().showListOfAlerts();
        });

        // The menu expands with all the alerts, and the dialog's scroll area allows
        // browsing it up and down.
        ScrollAreaWidget &area = self().area();
        alerts = new MenuWidget;
        alerts->enableScrolling(false);
        alerts->setGridSize(1, ui::Expand, 0, ui::Expand);
        alerts->rule().setLeftTop(area.contentRule().left(), area.contentRule().top());
        area.setContentSize(alerts->rule());
        area.add(alerts);

        area.enableIndicatorDraw(true);

        alerts->organizer().audienceForWidgetCreation() += this;
        alerts->organizer().audienceForWidgetUpdate() += this;

        // Set up the automatic hide timer.
        hideTimer += [this](){ self().hideNotification(); };
        hideTimer.setSingleShot(true);
        App::config(VAR_AUTOHIDE()).audienceForChange() += this;
    }

    NotificationAreaWidget &notifs()
    {
        return ClientWindow::main().notifications();
    }

    TimeSpan autoHideAfterSeconds() const
    {
        return App::config().getd(VAR_AUTOHIDE(), 3 * 60);
    }

    void variableValueChanged(Variable &, const Value &)
    {
        // Update the auto-hide timer.
        if (!fequal(autoHideAfterSeconds(), 0))
        {
            // Never autohide.
            hideTimer.stop();
        }
        else
        {
            hideTimer.setInterval(autoHideAfterSeconds());
            if (!hideTimer.isActive()) hideTimer.start();
        }
    }

    void queueAlert(const String &msg, Level level)
    {
        pending.put(new AlertItem(msg, level));
    }

    bool addPendingAlerts()
    {
        bool changed = false;
        while (AlertItem *alert = pending.take())
        {
            add(alert);
            changed = true;
        }
        // Remove excess alerts.
        while (alerts->items().size() > maxCount)
        {
            alerts->items().remove(alerts->items().size() - 1);
            changed = true;
        }
        return changed;
    }

    void add(AlertItem *alert)
    {
        DE_ASSERT_IN_MAIN_THREAD();

        // If we already have this, don't re-add.
        for (ui::Data::Pos i = 0; i < alerts->items().size(); ++i)
        {
            if (!alerts->items().at(i).label().compareWithoutCase(alert->label()))
                return;
        }

        alerts->items().insert(0, alert);
    }

    void widgetCreatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        // Background is provided by the popup.
        widget.set(Background());

        LabelWidget &label = widget.as<LabelWidget>();

        // Each alert has an icon identifying the originating subsystem and the level
        // of the alert.
        label.setBehavior(ContentClipping); // allow clip-culling
        label.setTextStyle(&styling);
        label.setMaximumTextWidth(rule("alerts.width").valuei());
        label.setSizePolicy(ui::Expand, ui::Expand);
        label.setAppearanceAnimation(LabelWidget::AppearGrowVertically, 0.5);
        label.setAlignment(ui::AlignBottom);
        label.setImage(style().images().image("alert"));
        label.setOverrideImageSize(style().fonts().font("default").height());
        label.setImageAlignment(ui::AlignTop);
        label.setTextAlignment(ui::AlignRight);
        label.margins()
                .setLeft("")
                .setRight("")
                .setBottom("");

        const AlertItem &alert = item.as<AlertItem>();
        switch (alert.level())
        {
        case Minor:
            label.setImageColor(Vec4f(style().colors().colorf("text"), .5f));
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

    void widgetUpdatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        DE_UNUSED(widget);
        DE_UNUSED(item);
    }

    void showNotification()
    {
        // Change color to indicate new alerts.
        notification->setImageColor(style().colors().colorf("accent"));

        notifs().showOrHide(*notification, true);

        // Restart the autohiding timer.
        if (autoHideAfterSeconds() > 0.0)
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
        if (alerts->items().isEmpty())
        {
            // No alerts to show.
            hideNotification();
            return true; // was hidden
        }
        return false;
    }

    void updateAutohideTimeSelection()
    {
        const ddouble time = autoHideAfterSeconds();
        ui::DataPos pos = autohideTimes->items().findData(NumberValue(time));
        if (pos != ui::Data::InvalidPos)
        {
            autohideTimes->setSelected(pos);
        }
        else
        {
            autohideTimes->setSelected(autohideTimes->items().findData(NumberValue(0)));
        }
    }
};

AlertDialog::AlertDialog(const String &/*name*/) : d(new Impl(this))
{
    setOutlineColor("transparent");

    // The dialog is connected to the notification icon.
    d->notification->setPopup(*this, ui::Down);

    buttons() << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default,
                                      "Clear All")
              << new DialogButtonItem(DialogWidget::ActionPopup | DialogWidget::Id1,
                                      style().images().image("gear"));

    // Auto-hide setting, positioned next to the Gear button.
    // These are not part of the dialog proper, but should be styled like regular
    // dialog contents.
    d->stylist.setContainer(*this);
    auto &gearButton = *popupButtonWidget(DialogWidget::Id1);

    gearButton.setPopup([](const PopupButtonWidget &) {
        return new LogSettingsDialog;
    }, ui::Left);
    gearButton.setOpener([this] (PopupWidget *pop) {
        auto &dlg = pop->as<LogSettingsDialog>();
        audienceForClose() += [pop](){ pop->close(); };
        dlg.orphan();
        dlg.exec(root());
    });

    auto *lab = LabelWidget::newWithText("Hide After:", this);

    add(d->autohideTimes = new ChoiceWidget);
    d->autohideTimes->items()
            << new ChoiceItem("1 min",   NumberValue(60))
            << new ChoiceItem("3 mins",  NumberValue(3 * 60))
            << new ChoiceItem("5 mins",  NumberValue(5 * 60))
            << new ChoiceItem("10 mins", NumberValue(10 * 60))
            << new ChoiceItem("Never",   NumberValue(0));
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

    d->autohideTimes->audienceForUserSelection() += [this]() { autohideTimeChanged(); };
}

void AlertDialog::newAlert(const String &message, Level level)
{
    d->queueAlert(message, level);
}

void AlertDialog::update()
{
    DialogWidget::update();

    if (d->addPendingAlerts())
    {
        d->showNotification();
    }
}

void AlertDialog::showListOfAlerts()
{
    if (isOpen() || d->hideIfEmpty()) return;

    // Restore the normal color.
    d->notification->setImageColor(style().colors().colorf("text"));

    area().scrollToTop(0.0);
    open();
}

void AlertDialog::hideNotification()
{
    d->hideNotification();
}

void AlertDialog::autohideTimeChanged()
{
    App::config().set(VAR_AUTOHIDE(), d->autohideTimes->selectedItem().data().asInt());
}

void AlertDialog::finish(int result)
{
    DialogWidget::finish(result);

    // The alerts will be cleared if the dialog is accepted.
    d->clearOnDismiss = (result != 0);
}

void AlertDialog::panelDismissed()
{
    if (d->clearOnDismiss)
    {
        d->clearOnDismiss = false;
        d->alerts->items().clear();
        d->hideIfEmpty();
    }

    DialogWidget::panelDismissed();
}
