/** @file sidebarwidget.cpp  Base class for sidebar widgets.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/dialogcontentstylist.h>
#include <de/sequentiallayout.h>

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(SidebarWidget)
, DE_OBSERVES(DoomsdayApp, GameChange)
{
    DialogContentStylist              stylist;
    GuiWidget *                       container;
    ScrollAreaWidget *                sidebarContent;
    IndirectRule *                    firstColumnWidth; ///< Shared by all groups.
    LabelWidget *                     title;
    ButtonWidget *                    close;
    std::unique_ptr<SequentialLayout> layout;

    Impl(Public *i)
        : Base(i)
        , firstColumnWidth(new IndirectRule)
    {
        DoomsdayApp::app().audienceForGameChange() += this;

        container = new GuiWidget;

        // The contents of the editor will scroll.
        sidebarContent = new ScrollAreaWidget;
        sidebarContent->enableIndicatorDraw(true);
        stylist.setContainer(*sidebarContent);
        container->add(sidebarContent);

        // Set up the editor UI.
        container->add(title = new LabelWidget);
        title->margins().set(rule("dialog.gap"));
        title->margins().setLeft("gap");
        title->setFont("title");
        title->setTextColor("accent");
        title->setSizePolicy(ui::Expand, ui::Expand);

        // Button for closing the sidebar.
        container->add(close = new ButtonWidget);
        close->margins().set(rule("dialog.gap"));
        close->setImage(style().images().image("close.ringless"));
        close->setImageColor(title->textColorf());
        close->setOverrideImageSize(title->font().height());
        close->setActionFn([this](){ self().close(); });
        close->setSizePolicy(ui::Expand, ui::Expand);
    }

    ~Impl()
    {
        releaseRef(firstColumnWidth);
    }

    void currentGameChanged(const Game &newGame)
    {
        if (newGame.isNull())
        {
            // Back to Home -- sidebars are not expected to remain open.
            self().close();
        }
    }
};

SidebarWidget::SidebarWidget(const String &titleText, const String &name)
    : PanelWidget(name)
    , d(new Impl(this))
{
    setSizePolicy(Fixed);
    setWaitForContentReady(false);
    setOpeningDirection(Left);
    setAnimationStyle(Smooth);
    set(Background(style().colors().colorf("background")).withSolidFillOpacity(1));

    d->title->setText(titleText);

    // Basic layout.
    d->title->rule()
            .setInput(Rule::Top,  d->container->rule().top())
            .setInput(Rule::Left, d->container->rule().left());
    d->close->rule()
            .setInput(Rule::Right,  d->container->rule().right())
            .setInput(Rule::Bottom, d->title->rule().bottom());

    d->sidebarContent->rule()
        .setInput(Rule::Left,   d->container->rule().left())
        .setInput(Rule::Width,  d->sidebarContent->contentRule().width() +
                                d->sidebarContent->margins().width())
        .setInput(Rule::Top,    d->title->rule().bottom())
        .setInput(Rule::Bottom, rule().bottom());
    
    d->container->rule().setSize(d->sidebarContent->rule().width(),
                                 rule().height());    
    setContent(d->container);

    const RuleRectangle &area = d->sidebarContent->contentRule();
    d->layout.reset(new SequentialLayout(area.left(), area.top(), Down));

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

const Rule &SidebarWidget::maximumOfAllGroupFirstColumns() const
{
    const Rule *max = nullptr;
    for (GuiWidget *child : d->sidebarContent->childWidgets())
    {
        if (auto *g = maybeAs<VariableGroupEditor>(child))
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
    return *d->sidebarContent;
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

void SidebarWidget::updateSidebarLayout(const Rule &minWidth,
                                        const Rule &extraHeight)
{
    d->firstColumnWidth->setSource(maximumOfAllGroupFirstColumns());

    d->sidebarContent->setContentSize(
            OperatorRule::maximum(minWidth, d->layout->width(), rule("sidebar.width")),
            d->layout->height() + extraHeight);
}
