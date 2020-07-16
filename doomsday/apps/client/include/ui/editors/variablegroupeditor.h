/** @file variablegroupeditor.h  Editor of a group of variables.
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

#ifndef DE_CLIENT_UI_EDITORS_VARIABLEGROUPEDITOR_H
#define DE_CLIENT_UI_EDITORS_VARIABLEGROUPEDITOR_H

#include <de/foldpanelwidget.h>
#include <de/scrollareawidget.h>
#include <de/variabletogglewidget.h>
#include <de/variablechoicewidget.h>
#include <de/variablesliderwidget.h>
#include <de/variablelineeditwidget.h>

#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/widgets/cvarsliderwidget.h"

/**
 * Editor for adjusting a group of variables.
 *
 * This widget has an unusual ownership for a couple of its subwidgets.
 * Ownership of both the title widget (created by the base class) and the reset
 * button (created by VariableGroupEditor) is given to the IOwner's container
 * widget. Call destroyAssociatedWidgets() to destroy these widgets manually.
 */
class VariableGroupEditor : public de::FoldPanelWidget
{
public:
    class IOwner
    {
    public:
        virtual const de::Rule &firstColumnWidthRule() const = 0;
        virtual de::ScrollAreaWidget &containerWidget() = 0;
        virtual void resetToDefaults(const de::String &name) = 0;

        virtual ~IOwner() = default;
    };

public:
    /**
     * Constructs a variable group editor.
     * @param owner      Owner (e.g., a sidebar).
     * @param name       Widget name.
     * @param titleText  Title for the group.
     * @param header     Widget to place above the variables. Takes ownership.
     */
    VariableGroupEditor(IOwner *owner, const de::String &name, const de::String &titleText,
                        GuiWidget *header = 0);

    /**
     * Destroys the title widget and the reset button, which are not owned
     * by this widget.
     */
    void destroyAssociatedWidgets();

    void setResetable(bool resetable);

    IOwner &owner();
    de::GuiWidget *header() const;
    de::ButtonWidget &resetButton();
    const de::Rule &firstColumnWidth() const;

    enum LabelType { SingleCell, EntireRow };

    void addSpace();
    de::LabelWidget *addLabel(const de::String &text, LabelType labelType = SingleCell);

    CVarToggleWidget *addToggle(const char *cvar, const de::String &label);
    CVarChoiceWidget *addChoice(const char *cvar, de::ui::Direction opening = de::ui::Up);
    CVarSliderWidget *addSlider(const char *cvar);
    CVarSliderWidget *addSlider(const char *cvar, const de::Ranged &range, double step, int precision);

    de::VariableToggleWidget *addToggle(de::Variable &var, const de::String &label);
    de::VariableSliderWidget *addSlider(de::Variable &var, const de::Ranged &range, double step, int precision);
    de::VariableLineEditWidget *addLineEdit(de::Variable &var);

    void addWidget(GuiWidget *widget);

    /**
     * Commit all added widgets to the group. This finalizes the layout of the
     * added widgets.
     */
    void commit();

    void fetch();

    // PanelWidget.
    void preparePanelForOpening();
    void panelClosing();

    virtual void resetToDefaults();
    virtual void foldAll();
    virtual void unfoldAll();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_EDITORS_VARIABLEGROUPEDITOR_H
