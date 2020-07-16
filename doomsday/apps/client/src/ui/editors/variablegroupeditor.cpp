/** @file variablegroupeditor.cpp  Editor of a group of variables.
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

#include "ui/editors/variablegroupeditor.h"

#include <de/popupmenuwidget.h>
#include <de/sequentiallayout.h>

using namespace de;

/**
 * Opens a popup menu for folding/unfolding all settings groups.
 */
struct RightClickHandler : public GuiWidget::IEventHandler
{
    VariableGroupEditor &editor;

    RightClickHandler(VariableGroupEditor *editor) : editor(*editor)
    {}

    bool handleEvent(GuiWidget &widget, const Event &event)
    {
        switch (widget.handleMouseClick(event, MouseEvent::Right))
        {
        case GuiWidget::MouseClickFinished: {
            PopupMenuWidget *pop = new PopupMenuWidget;
            pop->setDeleteAfterDismissed(true);
            editor.add(pop);
            pop->setAnchorAndOpeningDirection(widget.rule(), ui::Left);
            pop->items()
                    << new ui::ActionItem("Fold All",   [this](){ editor.foldAll(); })
                    << new ui::ActionItem("Unfold All", [this](){ editor.unfoldAll(); });
            pop->open();
            return true; }

        case GuiWidget::MouseClickUnrelated:
            return false;

        default:
            return true;
        }
    }
};

DE_PIMPL(VariableGroupEditor)
{
    IOwner *owner;
    bool resetable = false;
    SafeWidgetPtr<ButtonWidget> resetButton;
    GuiWidget *content;
    GuiWidget *header;
    GridLayout layout;
    const Rule *firstColumnWidth;

    Impl(Public *i, IOwner *owner)
        : Base(i)
        , owner(owner)
        , firstColumnWidth(0)
    {}

    ~Impl()
    {
        releaseRef(firstColumnWidth);
    }

    void foldAll(bool fold)
    {
        for (GuiWidget *child : owner->containerWidget().childWidgets())
        {
            if (auto *g = maybeAs<VariableGroupEditor>(child))
            {
                if (fold)
                    g->close(0.0);
                else
                    g->open();
            }
        }
    }
};

VariableGroupEditor::VariableGroupEditor(IOwner *owner, const String &name,
                                         const String &titleText, GuiWidget *header)
    : FoldPanelWidget(name)
    , d(new Impl(this, owner))
{
    d->content = new GuiWidget;
    setContent(d->content);

    makeTitle(titleText);

    // Set up a context menu for right-clicking.
    title().addEventHandler(new RightClickHandler(this));

    d->header = header;
    if (header)
    {
        d->content->add(header);
        header->rule()
                .setInput(Rule::Left,  d->content->rule().left())
                .setInput(Rule::Top,   d->content->rule().top())
                .setInput(Rule::Width, d->layout.width());
    }

    // We want the first column of all groups to be aligned with each other.
    d->layout.setColumnFixedWidth(0, owner->firstColumnWidthRule());

    d->layout.setGridSize(2, 0);
    d->layout.setColumnAlignment(0, ui::AlignRight);
    if (header)
    {
        d->layout.setLeftTop(d->content->rule().left(),
                             d->content->rule().top() + header->rule().height());
    }
    else
    {
        d->layout.setLeftTop(d->content->rule().left(), d->content->rule().top());
    }

    // Button for reseting this content to defaults.
    d->resetButton.reset(new ButtonWidget);
    d->resetButton->setText("Reset");
    d->resetButton->setActionFn([this](){ resetToDefaults(); });
    d->resetButton->rule()
            .setInput(Rule::Right,   d->owner->containerWidget().contentRule().right())
            .setInput(Rule::AnchorY, title().rule().top() + title().rule().height() / 2)
            .setAnchorPoint(Vec2f(0, .5f));
    d->resetButton->disable();

    d->owner->containerWidget().add(&title());
    d->owner->containerWidget().add(d->resetButton);
    d->owner->containerWidget().add(this);
}

void VariableGroupEditor::destroyAssociatedWidgets()
{
    GuiWidget::destroy(d->resetButton);
    GuiWidget::destroy(&title());
}

void VariableGroupEditor::setResetable(bool resetable)
{
    d->resetable = resetable;
}

GuiWidget *VariableGroupEditor::header() const
{
    return d->header;
}

ButtonWidget &VariableGroupEditor::resetButton()
{
    return *d->resetButton;
}

const Rule &VariableGroupEditor::firstColumnWidth() const
{
    return *d->firstColumnWidth;
}

void VariableGroupEditor::preparePanelForOpening()
{
    FoldPanelWidget::preparePanelForOpening();
    if (d->resetable)
    {
        d->resetButton->enable();
    }
}

void VariableGroupEditor::panelClosing()
{
    FoldPanelWidget::panelClosing();
    d->resetButton->disable();
}

void VariableGroupEditor::addSpace()
{
    d->layout << Const(0);
}

LabelWidget *VariableGroupEditor::addLabel(const String &text, LabelType labelType)
{
    LabelWidget *w = LabelWidget::newWithText(text, d->content);
    if (labelType == SingleCell)
    {
        d->layout << *w;
    }
    else
    {
        d->layout.setCellAlignment(Vec2i(0, d->layout.gridSize().y), ui::AlignLeft);
        d->layout.append(*w, 2);
    }
    return w;
}

CVarToggleWidget *VariableGroupEditor::addToggle(const char *cvar, const String &label)
{
    CVarToggleWidget *w = new CVarToggleWidget(cvar, label);
    d->content->add(w);
    d->layout << *w;
    return w;
}

CVarChoiceWidget *VariableGroupEditor::addChoice(const char *cvar, ui::Direction opening)
{
    CVarChoiceWidget *w = new CVarChoiceWidget(cvar);
    w->setOpeningDirection(opening);
    w->popup().useInfoStyle();
    d->content->add(w);
    d->layout << *w;
    return w;
}

CVarSliderWidget *VariableGroupEditor::addSlider(const char *cvar)
{
    auto *w = new CVarSliderWidget(cvar);
    d->content->add(w);
    d->layout << *w;
    return w;
}

CVarSliderWidget *VariableGroupEditor::addSlider(const char *cvar, const Ranged &range, double step, int precision)
{
    auto *w = addSlider(cvar);
    w->setRange(range, step);
    w->setPrecision(precision);
    return w;
}

VariableToggleWidget *VariableGroupEditor::addToggle(Variable &var, const String &label)
{
    auto *w = new VariableToggleWidget(label, var);
    d->content->add(w);
    d->layout << *w;
    return w;
}

VariableSliderWidget *VariableGroupEditor::addSlider(Variable &var, const Ranged &range, double step, int precision)
{
    auto *w = new VariableSliderWidget(var, range, step);
    w->setPrecision(precision);
    d->content->add(w);
    d->layout << *w;
    return w;
}

VariableLineEditWidget *VariableGroupEditor::addLineEdit(Variable &var)
{
    auto *w = new VariableLineEditWidget(var);
    d->content->add(w);
    d->layout << *w;
    w->rule().setInput(Rule::Width, rule("slider.width"));
    return w;
}

void VariableGroupEditor::addWidget(GuiWidget *widget)
{
    d->content->add(widget);
    d->layout << *widget;
}

void VariableGroupEditor::commit()
{
    d->content->rule().setSize(d->layout.width(), d->layout.height() +
                               (d->header? d->header->rule().height() : Const(0)));

    // Extend the title all the way to the button.
    title().rule().setInput(Rule::Right, d->resetButton->rule().left());

    // Calculate the maximum rule for the first column items.
    for (int i = 0; i < d->layout.gridSize().y; ++i)
    {
        const GuiWidget *w = d->layout.at(Vec2i(0, i));
        if (w && d->layout.widgetCellSpan(*w) == 1)
        {
            changeRef(d->firstColumnWidth,
                      OperatorRule::maximum(w->rule().width(), d->firstColumnWidth));
        }
    }
    if (d->header)
    {
        // Make sure the editor is wide enough to fit the entire header.
        d->content->rule().setInput(Rule::Width,
                OperatorRule::maximum(d->layout.width(), d->header->rule().width()));
    }
}

void VariableGroupEditor::fetch()
{
    for (GuiWidget *child : d->content->childWidgets())
    {
        if (ICVarWidget *w = maybeAs<ICVarWidget>(child))
        {
            w->updateFromCVar();
        }
    }
}

void VariableGroupEditor::resetToDefaults()
{
    for (GuiWidget *child : d->content->childWidgets())
    {
        if (ICVarWidget *w = maybeAs<ICVarWidget>(child))
        {
            d->owner->resetToDefaults(w->cvarPath());
            //d->settings.resetSettingToDefaults(w->cvarPath());
            w->updateFromCVar();
        }
    }
}

void VariableGroupEditor::foldAll()
{
    d->foldAll(true);
}

void VariableGroupEditor::unfoldAll()
{
    d->foldAll(false);
}
