/** @file columnwidget.h  Home column.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_UI_COLUMNWIDGET_H
#define DENG_CLIENT_UI_COLUMNWIDGET_H

#include "headerwidget.h"
#include <de/ScrollAreaWidget>

/**
 * Home column.
 *
 * Columns have a background, header, and content. ColumnWidth is responsible
 * for switching between active and deactive visual styles.
 */
class ColumnWidget : public de::GuiWidget
{
    Q_OBJECT

public:
    ColumnWidget(de::String const &name = "");

    void setBackgroundImage(de::DotPath const &imageId);

    de::ScrollAreaWidget &scrollArea();
    HeaderWidget &header();
    de::Rule const &maximumContentWidth() const;
    de::Variable *configVariable() const;

    virtual de::String tabHeading() const = 0;
    virtual de::String configVariableName() const;
    virtual void setHighlighted(bool highlighted);
    bool isHighlighted() const;

    // Events.
    bool dispatchEvent(de::Event const &event,
                       bool (de::Widget::*memberFunc)(de::Event const &)) override;

signals:
    void mouseActivity(QObject const *columnWidget);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_COLUMNWIDGET_H
