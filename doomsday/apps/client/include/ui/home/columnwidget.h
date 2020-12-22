/** @file columnwidget.h  Home column.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UI_COLUMNWIDGET_H
#define DE_CLIENT_UI_COLUMNWIDGET_H

#include "headerwidget.h"
#include <de/scrollareawidget.h>

/**
 * Home column.
 *
 * Columns have a background, header, and content. ColumnWidth is responsible
 * for switching between active and deactive visual styles.
 */
class ColumnWidget : public de::GuiWidget
{
public:
    DE_AUDIENCE(Activity, void mouseActivity(const ColumnWidget *columnWidget))

public:
    ColumnWidget(const de::String &name = {});

    void setBackgroundImage(const de::DotPath &imageId);

    de::ScrollAreaWidget &scrollArea();
    HeaderWidget &header();
    const de::Rule &maximumContentWidth() const;
    de::Variable *configVariable() const;

    virtual de::String tabHeading() const = 0;
    virtual int        tabShortcut() const; // DDKEY
    virtual de::String configVariableName() const;
    virtual void       setHighlighted(bool highlighted);
    bool               isHighlighted() const;

    // Events.
    bool dispatchEvent(const de::Event &event,
                       bool (de::Widget::*memberFunc)(const de::Event &)) override;
    void update() override;

protected:
    void updateStyle() override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_COLUMNWIDGET_H
