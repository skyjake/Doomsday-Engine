/** @file variablegroupeditor.cpp  Editor of a group of variables.
 *
 * @authors Copyright (c) 2013-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/PopupMenuWidget>
#include <de/SignalAction>

using namespace de;

/**
 * Opens a popup menu for folding/unfolding all settings groups.
 */
struct RightClickHandler : public GuiWidget::IEventHandler
{
    VariableGroupEditor &editor;

    RightClickHandler(VariableGroupEditor *editor) : editor(*editor)
    {}

    bool handleEvent(GuiWidget &widget, Event const &event)
    {
        switch(widget.handleMouseClick(event, MouseEvent::Right))
        {
        case GuiWidget::MouseClickFinished: {
            PopupMenuWidget *pop = new PopupMenuWidget;
            pop->setDeleteAfterDismissed(true);
            editor.add(pop);
            pop->setAnchorAndOpeningDirection(widget.rule(), ui::Left);
            pop->items()
                    << new ui::ActionItem(QObject::tr("Fold All"),   new SignalAction(&editor, SLOT(foldAll())))
                    << new ui::ActionItem(QObject::tr("Unfold All"), new SignalAction(&editor, SLOT(unfoldAll())));
            pop->open();
            return true; }

        case GuiWidget::MouseClickUnrelated:
            return false;

        default:
            return true;
        }
    }
};

DENG2_PIMPL(VariableGroupEditor)
{
    IOwner *owner;
    bool resetable = false;
    ButtonWidget *resetButton;
    GuiWidget *group;
    GridLayout layout;
    Rule const *firstColumnWidth;

    Instance(Public *i, IOwner *owner)
        : Base(i)
        , owner(owner)
        , firstColumnWidth(0)
    {}

    ~Instance()
    {
        releaseRef(firstColumnWidth);
    }

    void foldAll(bool fold)
    {
        foreach(Widget *child, owner->containerWidget().childWidgets())
        {
            if(auto *g = child->maybeAs<VariableGroupEditor>())
            {
                if(fold)
                    g->close(0);
                else
                    g->open();
            }
        }
    }
};

VariableGroupEditor::VariableGroupEditor(IOwner *owner, String const &name, String const &titleText)
    : FoldPanelWidget(name)
    , d(new Instance(this, owner))
{
    d->group = new GuiWidget;
    setContent(d->group);
    makeTitle(titleText);

    // Set up a context menu for right-clicking.
    title().addEventHandler(new RightClickHandler(this));

    // We want the first column of all groups to be aligned with each other.
    d->layout.setColumnFixedWidth(0, owner->firstColumnWidthRule());

    d->layout.setGridSize(2, 0);
    d->layout.setColumnAlignment(0, ui::AlignRight);
    d->layout.setLeftTop(d->group->rule().left(), d->group->rule().top());

    // Button for reseting this group to defaults.
    d->resetButton = new ButtonWidget;
    d->resetButton->setText(tr("Reset"));
    d->resetButton->setAction(new SignalAction(this, SLOT(resetToDefaults())));
    d->resetButton->rule()
            .setInput(Rule::Right,   d->owner->containerWidget().contentRule().right())
            .setInput(Rule::AnchorY, title().rule().top() + title().rule().height() / 2)
            .setAnchorPoint(Vector2f(0, .5f));
    d->resetButton->disable();

    d->owner->containerWidget().add(&title());
    d->owner->containerWidget().add(d->resetButton);
    d->owner->containerWidget().add(this);
}

void VariableGroupEditor::setResetable(bool resetable)
{
    d->resetable = resetable;
}

ButtonWidget &VariableGroupEditor::resetButton()
{
    return *d->resetButton;
}

Rule const &VariableGroupEditor::firstColumnWidth() const
{
    return *d->firstColumnWidth;
}

void VariableGroupEditor::preparePanelForOpening()
{
    FoldPanelWidget::preparePanelForOpening();
    if(d->resetable)
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

void VariableGroupEditor::addLabel(String const &text)
{
    d->layout << *LabelWidget::newWithText(text, d->group);
}

CVarToggleWidget *VariableGroupEditor::addToggle(char const *cvar, String const &label)
{
    CVarToggleWidget *w = new CVarToggleWidget(cvar, label);
    d->group->add(w);
    d->layout << *w;
    return w;
}

CVarChoiceWidget *VariableGroupEditor::addChoice(char const *cvar, ui::Direction opening)
{
    CVarChoiceWidget *w = new CVarChoiceWidget(cvar);
    w->setOpeningDirection(opening);
    w->popup().useInfoStyle();
    d->group->add(w);
    d->layout << *w;
    return w;
}

CVarSliderWidget *VariableGroupEditor::addSlider(char const *cvar)
{
    auto *w = new CVarSliderWidget(cvar);
    d->group->add(w);
    d->layout << *w;
    return w;
}

CVarSliderWidget *VariableGroupEditor::addSlider(char const *cvar, Ranged const &range, double step, int precision)
{
    auto *w = addSlider(cvar);
    w->setRange(range, step);
    w->setPrecision(precision);
    return w;
}

VariableSliderWidget *VariableGroupEditor::addSlider(Variable &var, Ranged const &range, double step, int precision)
{
    auto *w = new VariableSliderWidget(var, range, step);
    w->setPrecision(precision);
    d->group->add(w);
    d->layout << *w;
    return w;
}

void VariableGroupEditor::commit()
{
    d->group->rule().setSize(d->layout.width(), d->layout.height());

    // Extend the title all the way to the button.
    title().rule().setInput(Rule::Right, d->resetButton->rule().left());

    // Calculate the maximum rule for the first column items.
    for(int i = 0; i < d->layout.gridSize().y; ++i)
    {
        GuiWidget *w = d->layout.at(Vector2i(0, i));
        if(w)
        {
            changeRef(d->firstColumnWidth,
                      OperatorRule::maximum(w->rule().width(), d->firstColumnWidth));
        }
    }
}

void VariableGroupEditor::fetch()
{
    foreach(Widget *child, d->group->childWidgets())
    {
        if(ICVarWidget *w = child->maybeAs<ICVarWidget>())
        {
            w->updateFromCVar();
        }
    }
}

void VariableGroupEditor::resetToDefaults()
{
    foreach(Widget *child, d->group->childWidgets())
    {
        if(ICVarWidget *w = child->maybeAs<ICVarWidget>())
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
