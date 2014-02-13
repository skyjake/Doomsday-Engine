/** @file abstractlineeditor.h  Abstract line editor.
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

#ifndef LIBSHELL_ABSTRACTLINEEDITOR_H
#define LIBSHELL_ABSTRACTLINEEDITOR_H

#include "libshell.h"
#include "ITextEditor"
#include <de/libdeng2.h>
#include <de/String>
#include <de/Vector>

namespace de {
namespace shell {

class Lexicon;

/**
 * Abstract line editor.
 *
 * It is mandatory to provide a ILineWrapping instance that determines how the
 * text content gets wrapped onto multiple lines.
 *
 * The width of the editor is assumed to stay constant. A concrete
 * implementation will provide the current width via
 * AbstractLineEditor::maximumWidth().
 */
class LIBSHELL_PUBLIC AbstractLineEditor : public ITextEditor
{
public:
    AbstractLineEditor(ILineWrapping *lineWraps);

    ILineWrapping const &lineWraps() const;

    /**
     * Sets the prompt that is displayed in front of the edited text.
     *
     * @param promptText  Text for the prompt.
     */
    void setPrompt(String const &promptText);
    String prompt() const;

    void setText(String const &lineText);
    String text() const;

    void setCursor(int index);
    int cursor() const;

    /**
     * Determines the position of a specific character on the wrapped lines.
     * The Y component is the wrapped line index and the X component is the
     * character index on that line.
     */
    Vector2i linePos(int index) const;

    Vector2i lineCursorPos() const { return linePos(cursor()); }

    bool isSuggestingCompletion() const;
    Rangei completionRange() const;
    QStringList suggestedCompletions() const;
    void acceptCompletion();

    /**
     * Defines the terms and rules for auto-completion.
     *
     * @param lexicon  Lexicon.
     */
    void setLexicon(Lexicon const &lexicon);

    Lexicon const &lexicon() const;

    enum EchoMode {
        NormalEchoMode,
        PasswordEchoMode
    };

    /**
     * Determines how the entered text should be shown to the user.
     *
     * @param mode  Echo mode.
     */
    void setEchoMode(EchoMode mode);

    EchoMode echoMode() const;

    enum KeyModifier {
        Unmodified = 0,
        Shift      = 0x1,
        Control    = 0x2,
        Alt        = 0x4,
        Meta       = 0x8
    };
    Q_DECLARE_FLAGS(KeyModifiers, KeyModifier)

    virtual bool handleControlKey(int qtKey, KeyModifiers const &mods = Unmodified);

    /**
     * Inserts a fragment of text at the cursor position. The cursor moves
     * forward.
     *
     * @param text  Text to insert.
     */
    void insert(String const &text);

protected:
    ILineWrapping &lineWraps();

    /// Determines the available maximum width of text lines.
    virtual int maximumWidth() const = 0;

    // Notifications:
    virtual void numberOfLinesChanged(int lineCount) = 0;
    virtual void cursorMoved() = 0;
    virtual void contentChanged() = 0;
    virtual void autoCompletionBegan(String const &wordBase);
    virtual void autoCompletionEnded(bool accepted);

    enum LineWrapUpdateBehavior {
        RewrapNow,
        WrapUnlessWrappedAlready
    };

    /// Request rewrapping the text.
    void updateLineWraps(LineWrapUpdateBehavior behavior);

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractLineEditor::KeyModifiers)

} // namespace shell
} // namespace de

#endif // LIBSHELL_ABSTRACTLINEEDITOR_H
