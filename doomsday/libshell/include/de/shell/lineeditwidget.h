/** @file lineeditwidget.h  Widget for word-wrapped text editing.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_LINEEDITWIDGET_H
#define LIBSHELL_LINEEDITWIDGET_H

#include "libshell.h"
#include "TextWidget"

namespace de {
namespace shell {

/**
 * Widget for word-wrapped text editing.
 *
 * The widget adjusts its height automatically to fit to the full contents of
 * the edited, wrapped line.
 */
class LIBSHELL_PUBLIC LineEditWidget : public TextWidget
{
    Q_OBJECT

public:
    /**
     * The height rule of the widget is set up during construction.
     *
     * @param name  Widget name.
     */
    LineEditWidget(String const &name = "");

    virtual ~LineEditWidget();

    /**
     * Sets the prompt that is displayed in front of the edited text.
     *
     * @param promptText  Text for the prompt.
     */
    void setPrompt(String const &promptText);

    void setText(String const &lineText);
    String text() const;

    void setCursor(int index);
    int cursor() const;

    Vector2i cursorPosition();

    bool handleControlKey(int key);

    // Events.
    void viewResized();
    void draw();
    bool handleEvent(Event const *event);

signals:
    void enterPressed(String text);

private:
    struct Instance;
    Instance *d;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_LINEEDITWIDGET_H
