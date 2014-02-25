/** @file editorhistory.h  Text editor history buffer.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_EDITORHISTORY_H
#define LIBSHELL_EDITORHISTORY_H

#include "ITextEditor"

namespace de {
namespace shell {

/**
 * History buffer for a text editor. Remembers past entries entered into the
 * editor and allows navigation in them (bash-style).
 */
class LIBSHELL_PUBLIC EditorHistory
{
public:
    EditorHistory(ITextEditor *editor = 0);

    void setEditor(ITextEditor &editor);
    ITextEditor &editor();

    /**
     * Determines if the history is currently navigated to the latest/newest
     * entry.
     */
    bool isAtLatest() const;

    /**
     * Navigates to the latest entry in the history.
     */
    void goToLatest();

    /**
     * Enters the current editor contents into the history and clears the
     * editor.
     *
     * @return The entered text.
     */
    String enter();

    /**
     * Handles a key. History control keys include navigation in the history.
     *
     * @param qtKey  Qt key code.
     *
     * @return @c true, if key was handled.
     */
    bool handleControlKey(int qtKey);

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_ABSTRACTCOMMANDLINEEDITOR_H
