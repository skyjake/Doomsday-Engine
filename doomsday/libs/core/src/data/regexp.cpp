/** @file regexp.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/RegExp"

#include <c_plus/object.h>

namespace de {

RegExpMatch::RegExpMatch()
{
    clear();
}

const char *RegExpMatch::begin() const
{
    return match.subject + match.range.start;
}

const char *RegExpMatch::end() const
{
    return match.subject + match.range.end;
}

void RegExpMatch::clear()
{
    zap(match);
}

String RegExpMatch::captured(int index) const
{
    return String::take(captured_RegExpMatch(&match, index));
}

CString RegExpMatch::capturedCStr(int index) const
{
    iRangecc range;
    capturedRange_RegExpMatch(&match, index, &range);
    return range;
}

RegExp::RegExp(const String &expression, Sensitivity cs)
{
    _d = new_RegExp(expression, cs == CaseSensitive? caseSensitive_RegExpOption
                                                   : caseInsensitive_RegExpOption);
}

RegExp::~RegExp()
{
    iRelease(_d);
}

bool RegExp::exactMatch(const String &subject) const
{
    iRegExpMatch match;
    if (matchString_RegExp(_d, subject, &match))
    {
        return match.range.start == 0 && match.range.end == subject.sizei();
    }
    return false;
}

bool RegExp::match(const String &subject, RegExpMatch &match) const
{
    return matchString_RegExp(_d, subject, &match.match);
}

bool RegExp::hasMatch(const String &subject) const
{
    iRegExpMatch match;
    return matchString_RegExp(_d, subject, &match);
}

} // namespace de
