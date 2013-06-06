/** @file itexteditor.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_ITEXTEDITOR_H
#define LIBSHELL_ITEXTEDITOR_H

#include "libshell.h"

namespace de {
namespace shell {

/**
 * Interface for a text editor: text content and a cursor position.
 */
class ITextEditor
{
public:
    virtual ~ITextEditor() {}

    virtual void setText(String const &text) = 0;
    virtual void setCursor(int pos) = 0;

    virtual String text() const = 0;
    virtual int cursor() const = 0;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_ITEXTEDITOR_H
