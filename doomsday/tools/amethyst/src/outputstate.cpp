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

#include "outputstate.h"
#include "utils.h"
#include <QStringList>
#include <QDebug>

OutputState::OutputState(OutputContext *ctx) : Linkable()
{
    _context = ctx;
    _pos = 0;
    _marked = false;
    _alignment = OutputContext::AlignLeft;
    _cleanBreaks = true;
    if(!ctx) setRoot();
}

bool OutputState::isDone()
{
    return _pos == _context->output().size();
}

bool OutputState::allDone()
{
    for(OutputState *s = next(); !s->isRoot(); s = s->next())
        if(!s->isDone()) return false;
    return true;
}

OutputState *OutputState::findContext(OutputContext *ctx)
{
    for(OutputState *s = next(); !s->isRoot(); s = s->next())
        if(s->context() == ctx) return s;
    return NULL;
}

String OutputState::filledLine(const QStringList& /*completedLines*/)
{
    const String *source = &_context->output();
    int contextLen = source->size();
    int width = _context->width();
    int lastBreak = width, lastBreakPos;
    String line; // The line currently being formed.
    
    // We'll keep trying to print until we run out of characters
    // or the line is filled.
    while(_pos < contextLen && line.size() <= width)
    {
        QChar c = (*source)[_pos++];
        switch(c.toAscii())
        {
        case OutputContext::CtrlAlign:
            c = (*source)[_pos++];
            _alignment = c == 'L'? OutputContext::AlignLeft
                : c == 'R'? OutputContext::AlignRight
                : OutputContext::AlignCenter;
            break;

        case OutputContext::CtrlFill:
            c = (*source)[_pos++];
            while(line.size() < width) line += c;
            lastBreak = width;
            break;

        case OutputContext::CtrlUnderfill:
            if(!line.isEmpty())
                _pos--;
            else
            {
                int fillCount = trim(_previousFilledLine).size();
                c = (*source)[_pos++];
                while(fillCount-- > 0) line += c;
            }
            lastBreak = width;
            goto abortLine;

        case OutputContext::CtrlLinePrefixBegin:
            while((*source)[_pos++] != OutputContext::CtrlLinePrefixEnd) {}
            break;

        case OutputContext::CtrlCleanBreaks:
            break;

        case OutputContext::CtrlRawBreaks:
            break;

        case OutputContext::CtrlAnchor:
            break;

        case OutputContext::CtrlAnchorAppend:
        case OutputContext::CtrlAnchorPrepend:
            forever
            {
                c = (*source)[_pos++];
                if(c == OutputContext::CtrlAnchorAppend || c == OutputContext::CtrlAnchorPrepend)
                    break;
            }
            break;

        case '\t': // Space where the line may break.
            if(line.size() == width) goto abortLine;
            if(!line.isEmpty())
            {
                lastBreak = line.size();
                lastBreakPos = _pos;
                line += '\t';
            }
            break;

        case '\r': // Break to new line.
            lastBreak = width;
            goto abortLine;

        case '\n': // Break with one empty line in between.
            if(!line.isEmpty()) _pos--;
            lastBreak = width;
            goto abortLine;

        default:
            if(line.size() == width)
            {
                // We'll come back to this character.
                _pos--;
                goto abortLine;
            }
            // The character becomes part of the line.
            line += c;                  
            break;
        }
    }
abortLine:

    // Breat at the last suitable place, wind back pos.
    if(!isDone()
        && line.size() == width
        && lastBreak < width)
    {
        _pos = lastBreakPos;
        line.remove(lastBreak, line.size() - lastBreak);
    }

    // Remove extra breakable spaces in the end of the line.
    while(!line.isEmpty() && line[line.size() - 1] == '\t')
        line.remove(line.size() - 1, 1);

    // Convert breakable spaces to normal ones.
    line = replace(line, '\t', ' ');

    // Alignment, according to the current alignment mode.
    switch(_alignment)
    {
    default:
        while(line.size() < _context->width()) line += " ";
        break;

    case OutputContext::AlignRight:
        while(line.size() < _context->width()) line = " " + line;
        break;

    case OutputContext::AlignCenter:
        {
            int wl = _context->width() - line.size();
            line = QString(wl - wl/2, QChar(' ')) + line + QString(wl/2, QChar(' '));
            break;
        }
    }

    _previousFilledLine = line;
    return line;
}

static bool insertAnchorText(QString& line, QString text, bool prepend)
{
    int pos = line.lastIndexOf(QChar(OutputContext::CtrlAnchor));
    if(pos >= 0)
    {
        line.insert(prepend? pos : pos + 1, text);
        return true;
    }
    return false;
}

void OutputState::rawOutput(String& currentLine, String& linePrefix, QStringList& completedLines)
{    
    const String *source = &_context->output();
    int contextLen = source->size();

    // We'll keep going until we run out of characters.
    while(_pos < contextLen)
    {
        QChar c = (*source)[_pos++];
        switch(c.toAscii())
        {
        case OutputContext::CtrlAlign:
        case OutputContext::CtrlFill:
        case OutputContext::CtrlUnderfill:
            _pos++; // Ignore structural controls.
            break;

        case OutputContext::CtrlCleanBreaks:
            _cleanBreaks = true;
            break;

        case OutputContext::CtrlRawBreaks:
            _cleanBreaks = false;
            break;

        case OutputContext::CtrlLinePrefixBegin:
            // Set a new line prefix.
            linePrefix.clear();
            forever
            {
                c = (*source)[_pos++];
                if(c == OutputContext::CtrlLinePrefixEnd) break;
                linePrefix += c;
            }
            // Apply right away?
            if(currentLine.isEmpty()) currentLine = linePrefix;
            break;

        case OutputContext::CtrlAnchorPrepend:
        case OutputContext::CtrlAnchorAppend:
        {
            bool prepend = (c.toAscii() == OutputContext::CtrlAnchorPrepend);
            QString str;
            // Get the entire anchor string from the source.
            forever
            {
                c = (*source)[_pos++];
                if(c.toAscii() == OutputContext::CtrlAnchorAppend || c.toAscii() == OutputContext::CtrlAnchorPrepend)
                    break;
                str += c;
            }
            // Locate the latest anchor and insert the string there.
            if(!insertAnchorText(currentLine, str, prepend))
            {
                // No anchor on the current line, maybe on previous lines?
                for(int i = completedLines.size() - 1; i >= 0; i--)
                {
                    if(insertAnchorText(completedLines[i], str, prepend))
                        break;
                }
            }
            break;
        }

        case OutputContext::CtrlBreakingSpace: // Space where the line may break.
            if((linePrefix.isEmpty() && !currentLine.isEmpty() && !currentLine[currentLine.size() - 1].isSpace())
                    || !currentLine.endsWith(linePrefix))
            {
                currentLine += ' ';
            }
            break;

        case OutputContext::CtrlTab:
            currentLine += '\t';
            break;

        case OutputContext::CtrlLineBreak: // Break to new line.
            if(!_cleanBreaks || currentLine != linePrefix)
            {
                completedLines << currentLine;
            }
            currentLine = linePrefix;
            break;

        case OutputContext::CtrlParagraphBreak: // Break with one empty line in between.
            if(_cleanBreaks && currentLine == linePrefix)
            {
                // Check history.
                if(!completedLines.isEmpty() && completedLines.last() != linePrefix)
                    completedLines << linePrefix;
            }
            else
            {
                completedLines << currentLine << linePrefix;
            }
            currentLine = linePrefix;
            break;

        default:
            // The character becomes part of the line.
            currentLine += c;
            break;
        }
    }
}
