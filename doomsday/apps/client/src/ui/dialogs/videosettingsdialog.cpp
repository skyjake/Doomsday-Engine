/** @file videosettingsdialog.cpp  Dialog for video settings.
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

#include "ui/dialogs/videosettingsdialog.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/clientwindow.h"
#include "ui/clientwindowsystem.h"
#include "CommandAction"
#include "ConfigProfiles"
#include "clientapp.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>
#include <de/Config>
#include <de/VariableToggleWidget>
#include <de/VariableSliderWidget>
#include <de/ChoiceWidget>
#include <de/SequentialLayout>
#include <de/GridLayout>
#include <de/SignalAction>
#include <de/DisplayMode>
#include <QPoint>

using namespace de;
using namespace de::ui;

#if !defined (MACOSX)
#  define USE_REFRESH_RATE_CHOICE
#  define USE_COLOR_DEPTH_CHOICE
#endif

DENG2_PIMPL(VideoSettingsDialog)
#if !defined (DENG_MOBILE)
, DENG2_OBSERVES(PersistentGLWindow, AttributeChange)
#endif
{
    ClientWindow &win;
    VariableToggleWidget *showFps;
    ToggleWidget *fullscreen;
    ToggleWidget *maximized;
    ToggleWidget *centered;
    VariableToggleWidget *fsaa;
    VariableToggleWidget *vsync;
    ToggleWidget *fpsLimiter = nullptr;
    SliderWidget *fpsMax = nullptr;
    ChoiceWidget *modes = nullptr;
    ButtonWidget *windowButton = nullptr;
#ifdef USE_REFRESH_RATE_CHOICE
    ChoiceWidget *refreshRates;
#endif
#ifdef USE_COLOR_DEPTH_CHOICE
    ChoiceWidget *depths;
#endif
    ListData stretchChoices;
    CVarChoiceWidget *finaleAspect = nullptr;
    CVarChoiceWidget *hudAspect    = nullptr;
    CVarChoiceWidget *inludeAspect = nullptr;
    CVarChoiceWidget *menuAspect   = nullptr;

    Impl(Public *i)
        : Base(i)
        , win(ClientWindow::main())
    {
        ScrollAreaWidget &area = self().area();

        area.add(showFps      = new VariableToggleWidget(App::config("window.main.showFps")));
        area.add(fullscreen   = new ToggleWidget);
        area.add(maximized    = new ToggleWidget);
        area.add(centered     = new ToggleWidget);
        area.add(fsaa         = new VariableToggleWidget(App::config("window.main.fsaa")));
        area.add(vsync        = new VariableToggleWidget(App::config("window.main.vsync")));
        if (gotDisplayMode())
        {
            area.add(modes        = new ChoiceWidget("modes"));
            area.add(windowButton = new ButtonWidget);
        }
#ifdef USE_REFRESH_RATE_CHOICE
        area.add(refreshRates = new ChoiceWidget("refresh-rate"));
#endif
#ifdef USE_COLOR_DEPTH_CHOICE
        area.add(depths       = new ChoiceWidget("depths"));
#endif
#if !defined (DENG_MOBILE)
        win.audienceForAttributeChange() += this;
#endif

        area.add(fpsLimiter = new ToggleWidget);
        fpsLimiter->margins().setTop("dialog.separator");
        fpsLimiter->setText(tr("FPS Limiter"));
        QObject::connect(fpsLimiter, &ToggleWidget::stateChangedByUser, [this] (ToggleWidget::ToggleState st) {
            fpsMax->enable(st == ToggleWidget::Active);
            applyFpsMax();
        });

        area.add(fpsMax = new SliderWidget);
        fpsMax->margins().setTop("dialog.separator");
        fpsMax->setRange(Ranged(35, 60));
        fpsMax->setPrecision(0);
        fpsMax->rule().setInput(Rule::Width, centered->rule().width());
        QObject::connect(fpsMax, &SliderWidget::valueChangedByUser, [this] (double) { applyFpsMax(); });

        if (App_GameLoaded())
        {
            stretchChoices
                << new ChoiceItem(tr("Smart"),        SCALEMODE_SMART_STRETCH)
                << new ChoiceItem(tr("Original 1:1"), SCALEMODE_NO_STRETCH)
                << new ChoiceItem(tr("Stretched"),    SCALEMODE_STRETCH);

            area.add(finaleAspect = new CVarChoiceWidget("rend-finale-stretch"));
            area.add(hudAspect    = new CVarChoiceWidget("rend-hud-stretch"));
            area.add(inludeAspect = new CVarChoiceWidget("inlude-stretch"));
            area.add(menuAspect   = new CVarChoiceWidget("menu-stretch"));

            finaleAspect->setItems(stretchChoices);
            hudAspect   ->setItems(stretchChoices);
            inludeAspect->setItems(stretchChoices);
            menuAspect  ->setItems(stretchChoices);
        }
    }

    ~Impl()
    {
        // The common stretchChoices is being deleted now, before the widget tree.
        if (finaleAspect)
        {
            finaleAspect->useDefaultItems();
            hudAspect->useDefaultItems();
            inludeAspect->useDefaultItems();
            menuAspect->useDefaultItems();
        }

        //win.audienceForAttributeChange() -= this;
    }

    /**
     * Updates the widgets with the actual current state.
     */
    void fetch()
    {
#if !defined (DENG_MOBILE)
        fullscreen->setActive(win.isFullScreen());
        maximized->setActive(win.isMaximized());
        centered->setActive(win.isCentered());

        if (windowButton)
        {
            windowButton->enable(!win.isFullScreen() && !win.isMaximized());
        }

        if (gotDisplayMode())
        {
            // Select the current resolution/size in the mode list.
            GLWindow::Size current = win.fullscreenSize();

            // Update selected display mode.
            ui::Data::Pos closest = ui::Data::InvalidPos;
            int delta = 0;
            for (ui::Data::Pos i = 0; i < modes->items().size(); ++i)
            {
                QPoint const res = modes->items().at(i).data().toPoint();
                int dx = res.x() - current.x;
                int dy = res.y() - current.y;
                int d = dx*dx + dy*dy;
                if (closest == ui::Data::InvalidPos || d < delta)
                {
                    closest = i;
                    delta = d;
                }
            }
            modes->setSelected(closest);
        }

#ifdef USE_REFRESH_RATE_CHOICE
        refreshRates->setSelected(refreshRates->items().findData(int(win.refreshRate() * 10)));
#endif

#ifdef USE_COLOR_DEPTH_CHOICE
        // Select the current color depth in the depth list.
        depths->setSelected(depths->items().findData(win.colorDepthBits()));
#endif

#endif // !DENG_MOBILE

        // FPS limit.
        if (fpsMax)
        {
            const int max = Config::get().geti("window.main.maxFps", 0);
            fpsLimiter->setActive(max != 0);
            fpsMax->enable(max != 0);
            fpsMax->setValue(!max ? 35 : max);
        }

        foreach (GuiWidget *child, self().area().childWidgets())
        {
            if (ICVarWidget *cw = maybeAs<ICVarWidget>(child))
                cw->updateFromCVar();
        }
    }

#if !defined (DENG_MOBILE)
    void windowAttributesChanged(PersistentGLWindow &)
    {
        fetch();
    }
#endif
    
    void applyFpsMax()
    {
        if (fpsMax)
        {
            Config::get().set("window.main.maxFps",
                              fpsLimiter->isActive() ? int(fpsMax->value()) : 0);
        }
    }
    
    bool gotDisplayMode() const
    {
        return DisplayMode_Count() > 0;
    }
};

VideoSettingsDialog::VideoSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    heading().setText(tr("Video Settings"));
    heading().setImage(style().images().image("display"));

    // Toggles for video/window options.
    d->fullscreen->setText(tr("Fullscreen"));
    d->fullscreen->setAction(new CommandAction("togglefullscreen"));

    d->maximized->setText(tr("Maximized"));
    d->maximized->setAction(new CommandAction("togglemaximized"));

    d->centered->setText(tr("Center Window"));
    d->centered->setAction(new CommandAction("togglecentered"));

    d->showFps->setText(tr("Show FPS"));

    d->fsaa->setText(tr("Antialias"));

    d->vsync->setText(tr("VSync"));

#ifdef USE_REFRESH_RATE_CHOICE
    LabelWidget *refreshLabel = nullptr;
#endif
#ifdef USE_COLOR_DEPTH_CHOICE
    LabelWidget *colorLabel = nullptr;
#endif
    if (d->gotDisplayMode())
    {
        // Choice of display modes + 16/32-bit color depth.
        d->modes->setOpeningDirection(ui::Up);
        if (DisplayMode_Count() > 10)
        {
            d->modes->popup().menu().setGridSize(2, ui::Expand, 0, ui::Expand);
        }
        for (int i = 0; i < DisplayMode_Count(); ++i)
        {
            DisplayMode const *m = DisplayMode_ByIndex(i);
            QPoint const res(m->width, m->height);

            if (d->modes->items().findData(res) != ui::Data::InvalidPos)
            {
                // Got this already.
                continue;
            }

            String desc = String("%1 x %2 (%3:%4)")
                    .arg(m->width).arg(m->height)
                    .arg(m->ratioX).arg(m->ratioY);

            d->modes->items() << new ChoiceItem(desc, res);
        }

#ifdef USE_REFRESH_RATE_CHOICE
        {
            refreshLabel = LabelWidget::newWithText(tr("Monitor Refresh:"), &area());

            QSet<int> rates;
            rates.insert(0);
            for (int i = 0; i < DisplayMode_Count(); ++i)
            {
                rates.insert(int(DisplayMode_ByIndex(i)->refreshRate * 10));
            }
            foreach (int rate, rates)
            {
                if (rate == 0)
                {
                    d->refreshRates->items() << new ChoiceItem(tr("Default"), 0);
                }
                else
                {
                    d->refreshRates->items()
                        << new ChoiceItem(tr("%1 Hz").arg(float(rate) / 10.f, 0, 'f', 1), rate);
                }
            }
            d->refreshRates->items().sort([] (ui::Item const &a, ui::Item const &b) {
                int const i = a.data().toInt();
                int const j = b.data().toInt();
                if (!i) return true;
                if (!j) return false;
                return i < j;
            });
        }
#endif

#ifdef USE_COLOR_DEPTH_CHOICE
        {
            colorLabel = LabelWidget::newWithText(tr("Colors:"), &area());
            d->depths->items()
                << new ChoiceItem(tr("32-bit"), 32)
                << new ChoiceItem(tr("24-bit"), 24)
                << new ChoiceItem(tr("16-bit"), 16);
        }
#endif
    }

    buttons()
            << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    if (d->windowButton)
    {
        d->windowButton->setImage(style().images().image("window.icon"));
        d->windowButton->setOverrideImageSize(style().fonts().font("default").height());
        d->windowButton->setAction(new SignalAction(this, SLOT(showWindowMenu())));
    }

    // Layout all widgets.
    Rule const &gap = rule("dialog.gap");

    GridLayout layout(area().contentRule().left(),
                      area().contentRule().top(), GridLayout::RowFirst);
    layout.setGridSize(2, d->fpsLimiter? 4 : 3);
    layout.setColumnPadding(rule(RuleBank::UNIT));
    layout << *d->fsaa
           << *d->vsync
           << *d->showFps;
    if (d->fpsLimiter) layout << *d->fpsLimiter;
    layout << *d->fullscreen
           << *d->maximized
           << *d->centered;
    if (d->fpsMax) layout << *d->fpsMax;

    GridLayout modeLayout(d->vsync->rule().left(),
                          (d->fpsLimiter? d->fpsLimiter : d->showFps)->rule().bottom() + gap);
    modeLayout.setGridSize(2, 0);
    modeLayout.setColumnAlignment(0, ui::AlignRight);

    if (d->gotDisplayMode())
    {
        modeLayout << *LabelWidget::newWithText(tr("Resolution:"), &area());

        modeLayout.append(*d->modes, d->modes->rule().width() + d->windowButton->rule().width());

        d->windowButton->rule()
                .setInput(Rule::Top,  d->modes->rule().top())
                .setInput(Rule::Left, d->modes->rule().right());

#ifdef USE_REFRESH_RATE_CHOICE
        modeLayout << *refreshLabel << *d->refreshRates;
#endif
#ifdef USE_COLOR_DEPTH_CHOICE
        modeLayout << *colorLabel << *d->depths;
#endif
    }

    // Color adjustment.
    {
        auto *adjustButton = new ButtonWidget;
        adjustButton->setText(tr("Color Adjustments..."));
        adjustButton->setAction(new SignalAction(this, SLOT(showColorAdjustments())));
        area().add(adjustButton);

        modeLayout << Const(0) << *adjustButton;
    }

    if (d->inludeAspect)
    {
        // Aspect ratio options.
//        auto *aspectLabel = LabelWidget::newWithText(_E(D) + tr("Aspect Ratios"), &area());
//        aspectLabel->setFont("separator.label");
//        aspectLabel->margins().setTop("gap");
//        modeLayout.setCellAlignment(Vector2i(0, modeLayout.gridSize().y), ui::AlignLeft);
//        modeLayout.append(*aspectLabel, 2);
        LabelWidget::appendSeparatorWithText("Aspect Ratios", &area(), &modeLayout);
        modeLayout
                << *LabelWidget::newWithText(tr("Player Weapons:"), &area()) << *d->hudAspect
                << *LabelWidget::newWithText(tr("Intermissions:"), &area()) << *d->inludeAspect
                << *LabelWidget::newWithText(tr("Finales:"), &area()) << *d->finaleAspect
                << *LabelWidget::newWithText(tr("Menus:"), &area()) << *d->menuAspect;
    }

    area().setContentSize(OperatorRule::maximum(layout.width(), modeLayout.width()),
                          layout.height() + gap + modeLayout.height());

    d->fetch();

    if (d->gotDisplayMode())
    {
        connect(d->modes, SIGNAL(selectionChangedByUser(uint)), this, SLOT(changeMode(uint)));
    }

#ifdef USE_REFRESH_RATE_CHOICE
    connect(d->refreshRates, SIGNAL(selectionChangedByUser(uint)), this, SLOT(changeRefreshRate(uint)));
#endif
#ifdef USE_COLOR_DEPTH_CHOICE
    connect(d->depths, SIGNAL(selectionChangedByUser(uint)), this, SLOT(changeColorDepth(uint)));
#endif
}

void VideoSettingsDialog::resetToDefaults()
{
    ClientApp::windowSystem().settings().resetToDefaults();

    d->fetch();
}

#if !defined (DENG_MOBILE)

void VideoSettingsDialog::changeMode(uint selected)
{
    DENG2_ASSERT(d->modes);
    
    QPoint const res = d->modes->items().at(selected).data().toPoint();

    int const attribs[] = {
        ClientWindow::FullscreenWidth,  int(res.x()),
        ClientWindow::FullscreenHeight, int(res.y()),
        ClientWindow::End
    };

    d->win.changeAttributes(attribs);
}

void VideoSettingsDialog::changeColorDepth(uint selected)
{
#ifdef USE_COLOR_DEPTH_CHOICE
    Con_Executef(CMDS_DDAY, true, "setcolordepth %i",
                 d->depths->items().at(selected).data().toInt());
#else
    DENG2_UNUSED(selected);
#endif
}

void VideoSettingsDialog::changeRefreshRate(uint selected)
{
#ifdef USE_REFRESH_RATE_CHOICE
    float const rate = float(d->refreshRates->items().at(selected).data().toInt()) / 10.f;
    int const attribs[] = {
        ClientWindow::RefreshRate, int(rate * 1000), // milli-Hz
        ClientWindow::End
    };
    d->win.changeAttributes(attribs);
#else
    DENG2_UNUSED(selected);
#endif
}

void VideoSettingsDialog::showColorAdjustments()
{
    d->win.showColorAdjustments();
    d->win.taskBar().closeConfigMenu();
}

void VideoSettingsDialog::showWindowMenu()
{
    DENG2_ASSERT(d->windowButton);
    
    PopupMenuWidget *menu = new PopupMenuWidget;
    menu->setDeleteAfterDismissed(true);
    add(menu);

    menu->setAnchorAndOpeningDirection(d->windowButton->rule(), ui::Up);
    menu->items()
            << new ActionItem(tr("Apply to Window"),
                              new SignalAction(this, SLOT(applyModeToWindow())));
    menu->open();
}

void VideoSettingsDialog::applyModeToWindow()
{
    QPoint const res = d->modes->selectedItem().data().toPoint();

    int attribs[] = {
        ClientWindow::Width,  res.x(),
        ClientWindow::Height, res.y(),
        ClientWindow::End
    };

    d->win.changeAttributes(attribs);
}

#endif
