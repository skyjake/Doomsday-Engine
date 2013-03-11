/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AMETHYST_OUTPUT_STATE_H__
#define __AMETHYST_OUTPUT_STATE_H__

#include "linkable.h"
#include "outputcontext.h"
#include <QStringList>

class OutputState : public Linkable
{
public:
    OutputState(OutputContext *ctx = 0);

    OutputState *next() { return (OutputState*) Linkable::next(); }
    OutputState *prev() { return (OutputState*) Linkable::prev(); }
    OutputContext *context() { return _context; }

    /**
     * Call this for the root.
     */
    OutputState *findContext(OutputContext *ctx);
    
    void mark(bool setMark = true) { _marked = setMark; }
    bool isMarked() { return _marked; }
    bool isDone();
    bool allDone();
    int lineWidth() { return _context->width(); }

    /**
     * Outputs characters from the context to a line of characters. The characters
     * are removed from the context's line. The returned line is exactly as long as
     * the context is wide. Output will be structured and padded with spaces and
     * newlines for alignment and breaks. Used with fixed-width character output.
     *
     * @param completedLines  Repository of all the completed lines. This can be
     *                        accessed for information about preceding lines that
     *                        have already been printed.
     */
    String filledLine(const QStringList& completedLines);

    /**
     * Outputs characters from the context to a line of characters. The characters
     * are removed from the context's line. The entire contents of the context will
     * be output. Alignment and fills are ignored.
     *
     * @param currentLine     Buffer for the current line. Output is appended here.
     * @param linePrefix      String that is prefixed to every line.
     * @param completedLines  Repository of all the completed lines. This can be
     *                        accessed for information about preceding lines that
     *                        have already been printed.
     */
    void rawOutput(String& currentLine, String& linePrefix, QStringList& completedLines);

private:
    OutputContext* _context;
    int _pos;
    bool _marked;
    OutputContext::AlignMode _alignment;
    String _previousFilledLine;
    bool _cleanBreaks;
};

#endif
