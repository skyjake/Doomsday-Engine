/** @file monospacelogsinkformatter.cpp  Fixed-width log entry formatter.
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

#include "de/monospacelogsinkformatter.h"
#include "de/escapeparser.h"
#include "de/cstring.h"
#include "de/math.h"
#include "de/vector.h"

namespace de {

/**
 * Parses the tab escapes in a styled text and fills them using spaces. All lines of the
 * input text are handled, however they must be separated by newline characters.
 */
struct TabFiller
        : DE_OBSERVES(EscapeParser, PlainText)
        , DE_OBSERVES(EscapeParser, EscapeSequence)
{
    EscapeParser esc;
    StringList lines;
    String current;
    bool hasTabs;

    TabFiller(const String &text) : hasTabs(false)
    {
        esc.audienceForPlainText() += this;
        esc.audienceForEscapeSequence() += this;

        // Break the entire message into lines, excluding all escape codes
        // except for tabs.
        esc.parse(text);

        if (!current.isEmpty()) lines << current;
    }

    void handlePlainText(const CString &range)
    {
        for (mb_iterator i = range.begin(); i != range.end(); ++i)
        {
            const Char ch = *i;
            if (ch == '\n')
            {
                lines << current;
                current.clear();
            }
            else
            {
                current += ch;
            }
        }
    }

    void handleEscapeSequence(const CString &range)
    {
        mb_iterator it = range.begin();
        switch (*it)
        {
        case L'\t':
            current.append("\t+");
            hasTabs = true;
            break;

        case L'T':
            current.append('\t');
            it++;
            current.append(*it);
            hasTabs = true;
            break;

        default:
            break;
        }
    }

    int highestTabStop() const
    {
        int maxStop = 0;
        for (const String &ln : lines)
        {
            int stop = 0;
            for (auto i = BytePos(0); i < ln.size(); ++i)
            {
                if (ln.at(i) == '\t')
                {
                    ++i;
                    if (ln.at(i) == '+')
                    {
                        stop++;
                    }
                    else if (ln.at(i) == '`')
                    {
                        stop = 0;
                    }
                    else
                    {
                        stop = ln.at(i) - 'a';
                    }
                    maxStop = max(stop, maxStop);
                }
            }
        }
        return maxStop;
    }

    /// Returns @c true if all tabs were filled.
    bool fillTabs(StringList &fills, int maxStop, int minIndent) const
    {
        // The T` escape marks the place where tab stops are completely reset.
        struct { int fillIndex; BytePos pos; } resetAt = {-1, BytePos{}};

        for (int stop = 0; stop <= maxStop; ++stop)
        {
            int tabWidth = 0;

            // Find the widest position for this tab stop by checking all lines.
            for (int idx = 0; idx < fills.sizei(); ++idx)
            {
                const String &ln = fills.at(idx);
                int w = (idx > 0 ? minIndent : 0);
                iConstForEach(String, i, ln) // TODO: Use the C++ mb_iterator instead.
                {
                    if (i.value == '\t')
                    {
                        next_StringConstIterator(&i);
                        if (i.value == '`')
                        {
                            // Any tabs following this will need re-evaluating;
                            // continue to the tab-replacing phase.
                            goto replaceTabs;
                        }
                        if (i.value == '+' || Char(i.value).delta('a') == stop)
                        {
                            // This is it.
                            tabWidth = max(tabWidth, w);
                        }
                        continue;
                    }
                    else
                    {
                        ++w;
                    }
                }
            }

replaceTabs:
            // Fill up (replace) the tabs with spaces according to the widest found
            // position.
            for (int idx = 0; idx < fills.sizei(); ++idx)
            {
                String &ln = fills[idx];
                int w = (idx > 0? minIndent : 0);
                for (auto i = BytePos(0); i < ln.size(); ++i)
                {
                    if (ln.at(i) == '\t')
                    {
                        ++i;
                        if (ln.at(i) == '`')
                        {
                            // This T` escape will be removed once we've checked
                            // all the tab stops preceding it.
                            resetAt = {idx, i - 1};
                            goto nextStop;
                        }
                        if (ln.at(i) == '+' || ln.at(i) - 'a' == stop)
                        {
                            // Replace this stop with spaces.
                            ln.remove(--i, 2);
                            ln.insert(i, String(max(0, tabWidth - w), ' '));
                        }
                        continue;
                    }
                    else
                    {
                        ++w;
                    }
                }
            }
nextStop:;
        }

        // Now we can remove the possible T` escape.
        if (resetAt.fillIndex >= 0)
        {
            fills[resetAt.fillIndex].remove(resetAt.pos, 2);
            return false;
        }

        // All tabs removed.
        return true;
    }

    /// Returns the text with tabs replaced with spaces.
    /// @param minIndent  Identation for lines past the first one.
    String filled(int minIndent) const
    {
        // The fast way out.
        if (!hasTabs) return esc.plainText();

        const int maxStop = highestTabStop();

        StringList fills = lines;
        while (!fillTabs(fills, maxStop, minIndent)) {}
        return String::join(fills, "\n");
    }
};

MonospaceLogSinkFormatter::MonospaceLogSinkFormatter()
    : _maxLength(100), _minimumIndent(0), _sectionDepthOfPreviousLine(0)
{
#ifdef DE_DEBUG
    // Debug builds include a timestamp and msg type indicator.
    _maxLength = 125;
    _minimumIndent = 15;
#endif
}

StringList MonospaceLogSinkFormatter::logEntryToTextLines(const LogEntry &entry)
{
    using namespace std;

    const String &section = entry.section();

    StringList resultLines;
    dsize cutSection = 0;
#ifndef DE_DEBUG
    // In a release build we can dispense with the metadata.
    Flags entryFlags = LogEntry::Simple;
#else
    Flags entryFlags;
#endif

    // Compare the current entry's section with the previous one
    // and if there is an opportunity to omit or abbreviate.
    if (!_sectionOfPreviousLine.isEmpty()
            && entry.sectionDepth() >= 1
            && _sectionDepthOfPreviousLine <= entry.sectionDepth())
    {
        if (_sectionOfPreviousLine == section)
        {
            // Previous section is exactly the same, omit completely.
            entryFlags |= LogEntry::SectionSameAsBefore;
        }
        else if (section.beginsWith(_sectionOfPreviousLine))
        {
            // Previous section is partially the same, omit the common beginning.
            cutSection = _sectionOfPreviousLine.size();
            entryFlags |= LogEntry::SectionSameAsBefore;
        }
        else
        {
            CharPos prefix = section.commonPrefixLength(_sectionOfPreviousLine);
            if (prefix > 5)
            {
                // Some commonality with previous section, we can abbreviate
                // those parts of the section.
                entryFlags |= LogEntry::AbbreviateSection;
                cutSection = prefix.index; // NOTE: assumes sections are single-byte chars
            }
        }
    }

    // Fill tabs with space.
    String message = TabFiller(entry.asText(entryFlags, cutSection)).filled(_minimumIndent);

    // Remember for the next line.
    _sectionOfPreviousLine      = section;
    _sectionDepthOfPreviousLine = entry.sectionDepth();

    // The wrap indentation will be determined dynamically based on the content
    // of the line.
//    dsize wrapIndent     = wstring::npos;
//    dsize nextWrapIndent = wstring::npos;

    const String minIndentStr{dsize(_minimumIndent), ' '};

    using IndentedLine = std::pair<CString, dsize>;

    // Print line by line.
    List<IndentedLine> messageLines;
    for (const auto &hardLine : message.splitRef('\n'))
    {
        messageLines << IndentedLine{hardLine, 0};
    }

    /// @todo Ensure lines don't exceed the configured maximum length.
    /// Split messageLines; insert new entries with extra indent where needed.
    /// (Carry over indentation from previous log entry?)

    for (const auto &messageLine : messageLines)
    {
        String ln;
        if (resultLines)
        {
            // Lines other than the first one always get the minimum indentation.
            ln += minIndentStr;
        }
        if (messageLine.second > 0)
        {
            // Additional indentation.
            ln += String(messageLine.second, ' ');
        }
        ln += messageLine.first;
        resultLines << ln;
    }

#if 0
    dsize pos = 0;
    while (pos != wstring::npos)
    {
        // Find the length of the current line.
        dsize next = message.find(L'\n', pos);
        dsize lineLen = (next == wstring::npos ? (message.size() - pos) : (next - pos));
        const dsize maxLen = (pos > 0? _maxLength - wrapIndent : _maxLength);
        if (lineLen > maxLen)
        {
            // Wrap overly long lines.
            next = pos + maxLen;
            lineLen = maxLen;

            // Maybe there's whitespace we can wrap at.
            dsize checkPos = pos + maxLen;
            while (checkPos > pos)
            {
                /// @todo remove isPunct() and just check for the breaking chars
                if (iswspace(message[checkPos]) ||
                        (iswpunct(message[checkPos]) && message[checkPos] != '.' &&
                         message[checkPos] != ','    && message[checkPos] != '-' &&
                         message[checkPos] != '\''   && message[checkPos] != '"' &&
                         message[checkPos] != '('    && message[checkPos] != ')' &&
                         message[checkPos] != '['    && message[checkPos] != ']' &&
                         message[checkPos] != '_'))
                {
                    if (!iswspace(message[checkPos]))
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

        wstring lineText;

        //qDebug() << "[formatting]" << wrapIndent << lineText;

        // For lines other than the first one, print an indentation.
        if (pos > 0 && wrapIndent != String::npos)
        {
            lineText += wstring(wrapIndent, L' ');
        }

        // Crop this line's text out of the entire message.
        lineText += message.substr(pos, lineLen);

        // The wrap indent for this paragraph depends on the first line's content.
        const bool lineStartsWithSpace = lineText.empty() || iswspace(lineText.front());
        dsize firstNonSpace = wstring::npos;
        if (nextWrapIndent == wstring::npos && !lineStartsWithSpace)
        {
            dsize w = _minimumIndent;
            dsize firstBracket = wstring::npos;
            for (; w < lineText.size(); ++w)
            {
                // Indent to colons automatically (but not too deeply).
                if (w < lineText.size() - 1 && iswspace(lineText[w + 1]))
                {
                    if (firstBracket == wstring::npos && lineText[w] == ']')
                    {
                        firstBracket = w;
                    }
                }
                if (firstNonSpace == wstring::npos && !iswspace(lineText[w]))
                {
                    firstNonSpace = w;
                }
            }

            if (firstBracket != wstring::npos && firstBracket > 0)
            {
                nextWrapIndent = firstBracket + 2;
            }
            else if (firstNonSpace != wstring::npos && firstNonSpace > 0)
            {
                nextWrapIndent = firstNonSpace;
            }
            else
            {
                nextWrapIndent = _minimumIndent;
            }

            //qDebug() << "min" << _minimumIndent << "nsp" << firstNonSpace
            //         << "bct" << firstBracket;
        }
        //if (firstNonSpace < 0) firstNonSpace = _minimumIndent;

        // Check for formatting symbols.
        String finalLine{lineText};
        finalLine.replace(DE_ESC("R"), String(maxLen - _minimumIndent, '-'));
        resultLines << finalLine;

        // Advance to the next line.
        wrapIndent = nextWrapIndent;
        pos = next;
        if (pos != String::npos && pos > 0 && iswspace(message[pos]))
        {
            // At a forced newline, reset the wrap indentation.
            if (message[pos] == '\n')
            {
                //nextWrapIndent = -1;
                //wrapIndent = firstNonSpace;
            }
            pos++; // Skip whitespace.
        }
    }
#endif

    return resultLines;
}

void MonospaceLogSinkFormatter::setMaxLength(duint maxLength)
{
    _maxLength = de::max(duint(_minimumIndent + 10), maxLength);
}

duint MonospaceLogSinkFormatter::maxLength() const
{
    return _maxLength;
}

} // namespace de
