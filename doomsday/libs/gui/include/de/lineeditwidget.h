/** @file widgets/lineeditwidget.h
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBAPPFW_LINEEDITWIDGET_H
#define LIBAPPFW_LINEEDITWIDGET_H

#include "de/guiwidget.h"
#include <de/abstractlineeditor.h>
#include <de/keyevent.h>

namespace de {

/**
 * Widget showing a lineedit text and/or image.
 *
 * As a graphical widget, widget placement and line wrapping is handled in
 * terms of pixels rather than characters.
 *
 * Sets its own height based on the amount of content.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC LineEditWidget : public GuiWidget, public AbstractLineEditor
{
public:
    DE_AUDIENCE(Enter,         void enterPressed(const String &text))
    DE_AUDIENCE(ContentChange, void editorContentChanged(LineEditWidget &))

public:
    LineEditWidget(const String &name= {});

    void setText(const String &lineText) override;

    /**
     * Sets the text that will be shown in the editor when it is empty.
     *
     * @param hintText  Hint text.
     */
    void setEmptyContentHint(const String &hintText, const String &hintFont = {});

    /**
     * Enables or disables the signal emitted when the edit widget receives an
     * Enter key. By default, no a signal is emitted (and the key is thus not
     * eaten).
     *
     * @param enterSignal  @c true to enable signal and eat event, @c false to
     *                     disable.
     */
    void setSignalOnEnter(bool enterSignal);

    /**
     * Determines where the cursor is currently in view coordinates.
     */
    Rectanglei cursorRect() const;

    void setColorTheme(ColorTheme theme);
    void setUnfocusedBackgroundOpacity(float opacity);

    // Events.
    void viewResized() override;
    void focusGained() override;
    void focusLost() override;
    void update() override;
    void drawContent() override;
    bool handleEvent(const Event &event) override;

public:
    static term::Key termKey(const KeyEvent &keyEvent);

    static KeyModifiers modifiersFromKeyEvent(const KeyEvent::Modifiers &keyMods);

#if defined (DE_MOBILE)
protected slots:
    void userEnteredText(QString);
    void userFinishedTextEntry();
#endif

protected:
    void glInit() override;
    void glDeinit() override;
    void glMakeGeometry(GuiVertexBuilder &verts) override;
    void updateStyle() override;

    int maximumWidth() const override;
    void numberOfLinesChanged(int lineCount) override;
    void cursorMoved() override;
    void contentChanged() override;
    void autoCompletionEnded(bool accepted) override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_LINEEDITWIDGET_H
