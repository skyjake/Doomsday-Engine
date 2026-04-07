/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_LOGTEXTSTYLE_H
#define LIBCORE_LOGTEXTSTYLE_H

#include "de/libcore.h"

/**
 * @file logtextstyle.h
 * @internal Predefined text styles for log message text.
 */

namespace de {

extern const char *TEXT_STYLE_NORMAL;
extern const char *TEXT_STYLE_MESSAGE;
extern const char *TEXT_STYLE_MAJOR_MESSAGE;
extern const char *TEXT_STYLE_MINOR_MESSAGE;
extern const char *TEXT_STYLE_SECTION;
extern const char *TEXT_STYLE_MINOR_SECTION;
extern const char *TEXT_STYLE_MAJOR_SECTION;
extern const char *TEXT_STYLE_LOG_TIME;
extern const char *TEXT_MARK_INDENT;

} // namespace de

#endif // LIBCORE_LOGTEXTSTYLE_H
