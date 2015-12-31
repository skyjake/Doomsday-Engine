/** @file variablegroupeditor.h  Editor of a group of variables.
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

#ifndef DENG_CLIENT_UI_EDITORS_VARIABLEGROUPEDITOR_H
#define DENG_CLIENT_UI_EDITORS_VARIABLEGROUPEDITOR_H

#include <de/FoldPanelWidget>
#include <de/ScrollAreaWidget>
#include <de/VariableToggleWidget>
#include <de/VariableChoiceWidget>
#include <de/VariableSliderWidget>
#include <de/VariableLineEditWidget>

#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/widgets/cvarsliderwidget.h"

/**
 * Editor for adjusting a group of variables.
 */
class VariableGroupEditor : public de::FoldPanelWidget
{
    Q_OBJECT

public:
    class IOwner
    {
    public:
        virtual de::Rule const &firstColumnWidthRule() const = 0;
        virtual de::ScrollAreaWidget &containerWidget() = 0;
        virtual void resetToDefaults(de::String const &name) = 0;
    };

public:
    VariableGroupEditor(IOwner *owner, de::String const &name, de::String const &titleText);

    void setResetable(bool resetable);

    IOwner &owner();
    de::ButtonWidget &resetButton();

    de::Rule const &firstColumnWidth() const;
    void preparePanelForOpening();
    void panelClosing();

    void addSpace();
    void addLabel(de::String const &text);

    CVarToggleWidget *addToggle(char const *cvar, de::String const &label);
    CVarChoiceWidget *addChoice(char const *cvar, de::ui::Direction opening = de::ui::Up);
    CVarSliderWidget *addSlider(char const *cvar);
    CVarSliderWidget *addSlider(char const *cvar, de::Ranged const &range, double step, int precision);

    de::VariableSliderWidget *addSlider(de::Variable &var, de::Ranged const &range, double step, int precision);

    /**
     * Commit all added widgets to the group. This finalizes the layout of the
     * added widgets.
     */
    void commit();

    void fetch();

public slots:
    virtual void resetToDefaults();
    virtual void foldAll();
    virtual void unfoldAll();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_EDITORS_VARIABLEGROUPEDITOR_H
