/** @file editorhistory.h  Text editor history buffer.
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

#pragma once

#include "de/itexteditor.h"
#include "de/term/keyevent.h"

namespace de {

/**
 * History buffer for a text editor. Remembers past entries entered into the
 * editor and allows navigation in them (bash-style).
 *
 * @ingroup abstractUi
 */
class DE_PUBLIC EditorHistory
{
public:
    EditorHistory(ITextEditor *editor = nullptr);

    void         setEditor(ITextEditor &editor);
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
     * @param key  Key code.
     *
     * @return @c true, if key was handled.
     */
    bool handleControlKey(term::Key key);

    /**
     * Returns the history contents.
     *
     * @param maxCount  Maximum number of history entries. 0 for unlimited.
     */
    StringList fullHistory(int maxCount = 0) const;

    void setFullHistory(const StringList& history);

private:
    DE_PRIVATE(d)
};

} // namespace de
