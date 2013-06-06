/** @file lineeditwidget.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_LINEEDITWIDGET_H
#define DENG_CLIENT_LINEEDITWIDGET_H

#include "guiwidget.h"
#include <de/shell/AbstractLineEditor>

/**
 * Widget showing a lineedit text and/or image.
 *
 * As a graphical widget, widget placement and line wrapping is handled in
 * terms of pixels rather than characters.
 *
 * @ingroup gui
 */
class LineEditWidget : public GuiWidget, public de::shell::AbstractLineEditor
{
public:
    LineEditWidget(de::String const &name = "");

    /**
     * Sets the text that will be shown in the editor when it is empty.
     *
     * @param hintText  Hint text.
     */
    void setEmptyContentHint(de::String const &hintText);

    // Events.
    void viewResized();
    void focusGained();
    void focusLost();
    void update();
    void draw();
    bool handleEvent(de::Event const &event);

protected:
    void glInit();
    void glDeinit();
    void glMakeGeometry(DefaultVertexBuf::Builder &verts);
    void updateStyle();

    int maximumWidth() const;
    void numberOfLinesChanged(int lineCount);
    void cursorMoved();
    void contentChanged();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_LINEEDITWIDGET_H
