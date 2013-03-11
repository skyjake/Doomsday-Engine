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

#ifndef __AMETHYST_UTILS_H__
#define __AMETHYST_UTILS_H__

#include "string.h"

class Gem;

enum StringCasing
{
    LOWERCASE_ALL,
    UPPERCASE_ALL,
    CAPITALIZE_WORDS,
    CAPITALIZE_SENTENCE
};

enum FilterApplyMode
{
    ApplyNormal,
    ApplyPre,
    ApplyPost,
    ApplyAnchorPrepend,
    ApplyAnchorAppend
};

enum ArgType
{ 
    ArgShard, 
    ArgBlock, 
    ArgToken 
};

int visualSize(const String& str);
String stringCase(const String& src, StringCasing casing);
String stringInterlace(const String& src, QChar c = QChar(' '));
bool cutBefore(String &str, const String& pat);
bool cutAfter(String &str, const String& pat);
String alphaCounter(int num);
String romanCounter(int num);
String romanFilter(const String& src, bool upperCase = true);
String applyFilter(String input, const String& filter, FilterApplyMode mode, Gem *gem);
ArgType interpretArgType(const String& types, int index);
int styleForName(const String& name);
String nameForStyle(int flag);
String replace(const String& str, QChar ch, QChar withCh);
String trimLeft(const String& str);
String trimRight(const String& str);
String trimRightSpaceOnly(const String& str);
String trim(const String& str);
String dateString(String format = "yyyy-MM-dd");
bool fileFound(const String& fileName);

#endif
