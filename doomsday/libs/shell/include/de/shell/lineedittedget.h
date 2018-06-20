/** @file shell/lineeditwidget.h  Widget for word-wrapped text editing.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_LINEEDITTEDGET_H
#define LIBSHELL_LINEEDITTEDGET_H

#include "libshell.h"
#include "Tedget"
#include "AbstractLineEditor"

namespace de { namespace shell {

/**
 * Widget for word-wrapped text editing.
 *
 * The widget adjusts its height automatically to fit to the full contents of
 * the edited, wrapped lines.
 *
 * @ingroup textUi
 */
class LIBSHELL_PUBLIC LineEditTedget
    : public Tedget
    , public AbstractLineEditor
{
public:
    DE_DEFINE_AUDIENCE2(Enter, void enterPressed(String text))

public:
    /**
     * The height rule of the widget is set up during construction.
     *
     * @param name  Widget name.
     */
    LineEditTedget(String const &name = String());

    /**
     * Enables or disables the signal emitted when the edit widget receives an
     * Enter key. By default, a signal is emitted.
     *
     * @param enterSignal  @c true to enable signal, @c false to disable.
     */
    void setSignalOnEnter(int enterSignal);

    Vec2i cursorPosition() const;

    bool handleControlKey(Key key, KeyModifiers const &mods = Unmodified);

    // Events.
    void viewResized();
    void update();
    void draw();
    bool handleEvent(Event const &event);

protected:
    virtual int  maximumWidth() const;
    virtual void numberOfLinesChanged(int lineCount);
    virtual void cursorMoved();
    virtual void contentChanged();

private:
    DE_PRIVATE(d)
};

}} // namespace de::shell

#endif // LIBSHELL_LINEEDITTEDGET_H
