/** @file monospacelogsinkformatter.cpp  Fixed-width log entry formatter.
 * @ingroup core
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

#include "de/MonospaceLogSinkFormatter"

namespace de {

MonospaceLogSinkFormatter::MonospaceLogSinkFormatter()
    : _maxLength(89), _minimumIndent(0), _sectionDepthOfPreviousLine(0)
{
#ifdef DENG2_DEBUG
    // Debug builds include a timestamp and msg type indicator.
    _maxLength = 110;
    _minimumIndent = 25;
#endif
}

QList<String> MonospaceLogSinkFormatter::logEntryToTextLines(LogEntry const &entry)
{
    QList<String> resultLines;

    String const &section = entry.section();

    int cutSection = 0;

#ifndef DENG2_DEBUG
    // In a release build we can dispense with the metadata.
    LogEntry::Flags entryFlags = LogEntry::Simple;
#else
    LogEntry::Flags entryFlags;
#endif

    // Compare the current entry's section with the previous one
    // and if there is an opportunity to omit or abbreviate.
    if(!_sectionOfPreviousLine.isEmpty()
            && entry.sectionDepth() >= 1
            && _sectionDepthOfPreviousLine <= entry.sectionDepth())
    {
        if(_sectionOfPreviousLine == section)
        {
            // Previous section is exactly the same, omit completely.
            entryFlags |= LogEntry::SectionSameAsBefore;
        }
        else if(section.startsWith(_sectionOfPreviousLine))
        {
            // Previous section is partially the same, omit the common beginning.
            cutSection = _sectionOfPreviousLine.size();
            entryFlags |= LogEntry::SectionSameAsBefore;
        }
        else
        {
            int prefix = section.commonPrefixLength(_sectionOfPreviousLine);
            if(prefix > 5)
            {
                // Some commonality with previous section, we can abbreviate
                // those parts of the section.
                entryFlags |= LogEntry::AbbreviateSection;
                cutSection = prefix;
            }
        }
    }
    String message = entry.asText(entryFlags, cutSection);

    // Remember for the next line.
    _sectionOfPreviousLine      = section;
    _sectionDepthOfPreviousLine = entry.sectionDepth();

    // The wrap indentation will be determined dynamically based on the content
    // of the line.
    int wrapIndent = -1;
    int nextWrapIndent = -1;

    // Print line by line.
    String::size_type pos = 0;
    while(pos != String::npos)
    {
        // Find the length of the current line.
        String::size_type next = message.indexOf('\n', pos);
        duint lineLen = (next == String::npos? message.size() - pos : next - pos);
        duint const maxLen = (pos > 0? _maxLength - wrapIndent : _maxLength);
        if(lineLen > maxLen)
        {
            // Wrap overly long lines.
            next = pos + maxLen;
            lineLen = maxLen;

            // Maybe there's whitespace we can wrap at.
            int checkPos = pos + maxLen;
            while(checkPos > pos)
            {
                /// @todo remove isPunct() and just check for the breaking chars
                if(message[checkPos].isSpace() ||
                        (message[checkPos].isPunct() && message[checkPos] != '.' &&
                         message[checkPos] != ','    && message[checkPos] != '-' &&
                         message[checkPos] != '\''   && message[checkPos] != '"' &&
                         message[checkPos] != '('    && message[checkPos] != ')' &&
                         message[checkPos] != '['    && message[checkPos] != ']' &&
                         message[checkPos] != '_'))
                {
                    if(!message[checkPos].isSpace())
                    {
                        // Include the punctuation on this line.
                        checkPos++;
                    }

                    // Break here.
                    next = checkPos;
                    lineLen = checkPos - pos;
                    break;
                }
                checkPos--;
            }
        }

        // Crop this line's text out of the entire message.
        String lineText = message.substr(pos, lineLen);

        // For lines other than the first one, print an indentation.
        if(pos > 0)
        {
            lineText = QString(wrapIndent, QChar(' ')) + lineText;
        }

        // The wrap indent for this paragraph depends on the first line's content.
        if(nextWrapIndent < 0)
        {
            int w = _minimumIndent;
            int firstNonSpace = -1;
            for(; w < lineText.size(); ++w)
            {
                if(firstNonSpace < 0 && !lineText[w].isSpace())
                    firstNonSpace = w;

                // Indent to colons automatically (but not too deeply).
                if(lineText[w] == ':' && w < lineText.size() - 1 && lineText[w + 1].isSpace())
                    firstNonSpace = (w < int(_maxLength)*2/3? -1 : _minimumIndent);
            }

            nextWrapIndent = qMax(_minimumIndent, firstNonSpace);
        }

        // Check for formatting symbols.
        lineText.replace(DENG2_STR_ESCAPE("R"), String(maxLen - _minimumIndent, '-'));

        resultLines.append(lineText);

        // Advance to the next line.
        wrapIndent = nextWrapIndent;
        pos = next;
        if(pos != String::npos && message[pos].isSpace())
        {
            // At a forced newline, reset the wrap indentation.
            if(message[pos] == '\n')
            {
                nextWrapIndent = -1;
                wrapIndent = _minimumIndent;
            }
            pos++; // Skip whitespace.
        }
    }

    return resultLines;
}

} // namespace de
