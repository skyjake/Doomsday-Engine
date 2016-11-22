/** @file sidebarwidget.cpp  Base class for sidebar widgets.
 *
 * @authors Copyright (c) 2015-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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


#include "ui/widgets/sidebarwidget.h"
#include "ui/editors/variablegroupeditor.h"
#include "ui/clientwindow.h"

#include <doomsday/doomsdayapp.h>

#include <de/DialogContentStylist>
#include <de/SequentialLayout>
#include <de/SignalAction>

using namespace de;
using namespace de::ui;

DENG_GUI_PIMPL(SidebarWidget)
, DENG2_OBSERVES(DoomsdayApp, GameChange)
{
    DialogContentStylist stylist;
    ScrollAreaWidget *container;
    IndirectRule *firstColumnWidth; ///< Shared by all groups.
    LabelWidget *title;
    ButtonWidget *close;
    std::unique_ptr<SequentialLayout> layout;

    Impl(Public *i)
        : Base(i)
        , firstColumnWidth(new IndirectRule)
    {
        DoomsdayApp::app().audienceForGameChange() += this;

        // The contents of the editor will scroll.
        container = new ScrollAreaWidget;
        container->enableIndicatorDraw(true);
        stylist.setContainer(*container);

        container->add(close = new ButtonWidget);

        close->setImage(style().images().image("close.ringless"));
        close->setImageColor(style().colors().colorf("accent"));
        close->setOverrideImageSize(style().fonts().font("title").height().valuei());
        close->setAction(new SignalAction(thisPublic, SLOT(close())));
    }

    ~Impl()
    {
        releaseRef(firstColumnWidth);
    }

    void currentGameChanged(Game const &newGame)
    {
        if (newGame.isNull())
        {
            // Back to Home -- sidebars are not expected to remain open.
            self.close();
        }
    }
};

SidebarWidget::SidebarWidget(String const &titleText, String const &name)
    : PanelWidget(name)
    , d(new Impl(this))
{
    setSizePolicy(Fixed);
    setWaitForContentReady(false);
    setOpeningDirection(Left);
    set(Background(style().colors().colorf("background")).withSolidFillOpacity(1));

    // Set up the editor UI.
    d->title = LabelWidget::newWithText(titleText, d->container);
    d->title->setFont("title");
    d->title->setTextColor("accent");

    // Basic layout.
    RuleRectangle const &area = d->container->contentRule();
    d->title->rule()
            .setInput(Rule::Top,  area.top())
            .setInput(Rule::Left, area.left());
    d->close->rule()
            .setInput(Rule::Right,  area.right())
            .setInput(Rule::Bottom, d->title->rule().bottom());

    d->layout.reset(new SequentialLayout(area.left(), d->title->rule().bottom(), Down));

    d->container->rule().setSize(d->container->contentRule().width() +
                                 d->container->margins().width(),
                                 rule().height());
    setContent(d->container);

    // Install the editor.
    ClientWindow::main().setSidebar(ClientWindow::RightEdge, this);
}

SequentialLayout &SidebarWidget::layout()
{
    return *d->layout;
}

LabelWidget &SidebarWidget::title()
{
    return *d->title;
}

Rule const &SidebarWidget::maximumOfAllGroupFirstColumns() const
{
    Rule const *max = nullptr;
    foreach (Widget *child, d->container->childWidgets())
    {
        if (auto *g = child->maybeAs<VariableGroupEditor>())
        {
            changeRef(max, OperatorRule::maximum(g->firstColumnWidth(), max));
        }
    }
    if (!max)
    {
        return ConstantRule::zero();
    }
    return *refless(max);
}

IndirectRule &SidebarWidget::firstColumnWidth()
{
    return *d->firstColumnWidth;
}

ScrollAreaWidget &SidebarWidget::containerWidget()
{
    return *d->container;
}

ButtonWidget &SidebarWidget::closeButton()
{
    return *d->close;
}

void SidebarWidget::preparePanelForOpening()
{
    PanelWidget::preparePanelForOpening();
}

void SidebarWidget::panelDismissed()
{
    PanelWidget::panelDismissed();
    ClientWindow::main().unsetSidebar(ClientWindow::RightEdge);
}

void SidebarWidget::updateSidebarLayout(de::Rule const &minWidth,
                                        de::Rule const &extraHeight)
{
    d->firstColumnWidth->setSource(maximumOfAllGroupFirstColumns());

    d->container->setContentSize(OperatorRule::maximum(minWidth,
                                                       d->layout->width(),
                                                       rule("sidebar.width")),
                                 d->title->rule().height() +
                                 d->layout->height() +
                                 extraHeight);
}
