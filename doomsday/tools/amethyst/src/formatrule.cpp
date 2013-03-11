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

#include "formatrule.h"

FormatRule::FormatRule()
    : Rule(), _hasPre(false), _hasPost(false),
      _hasAnchorPrepend(false), _hasAnchorAppend(false)
{
    _type = FORMAT;
}

FormatRule::FormatRule(const String& formatString) : Rule()
{
    _type = FORMAT;
    _format = formatString;
    _hasPre = _format.contains("@<");
    _hasPost = _format.contains("@>");
    _hasAnchorPrepend = _format.contains("@]");
    _hasAnchorAppend = _format.contains("@[");
}

/**
 * Apply the formatting of the rule to the gem.
 *
 * @return  Returns the result of the formatting as it will appear in the output.
 */
String FormatRule::apply(FilterApplyMode mode, String input, Gem *gem)
{
    return applyFilter(input, _format, mode, gem);
}

