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

#ifndef __AMETHYST_FORMAT_RULE_H__
#define __AMETHYST_FORMAT_RULE_H__

#include "rule.h"
#include "outputcontext.h"
#include "utils.h"

class FormatRule : public Rule
{
public:
    FormatRule();
    FormatRule(const String& formatString);

    bool hasPre() const { return _hasPre; }
    bool hasPost() const { return _hasPost; }
    bool hasAnchorPrepend() const { return _hasAnchorPrepend; }
    bool hasAnchorAppend() const { return _hasAnchorAppend; }
    String apply(FilterApplyMode mode, String input, Gem *gem);

protected:
    String _format;
    bool _hasPre, _hasPost;
    bool _hasAnchorPrepend;
    bool _hasAnchorAppend;
};

#endif
