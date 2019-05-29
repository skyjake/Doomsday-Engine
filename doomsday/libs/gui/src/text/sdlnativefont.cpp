/** @file sdlnativefont.cpp
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "sdlnativefont.h"
#include <SDL_ttf.h>

namespace de {

DE_PIMPL_NOREF(SdlNativeFont)
{
    Impl()
    {}

    Impl(const Impl &)
    {}
};

SdlNativeFont::SdlNativeFont(const String &family)
    : NativeFont(family), d(new Impl)
{}

SdlNativeFont::SdlNativeFont(const SdlNativeFont &other)
    : NativeFont(other), d(new Impl(*other.d))
{}

SdlNativeFont &SdlNativeFont::operator=(const SdlNativeFont &other)
{
    NativeFont::operator=(other);
    d.reset(new Impl(*other.d));
    return *this;
}

void SdlNativeFont::commit() const
{}

int SdlNativeFont::nativeFontAscent() const
{
    return 0;
}
    
int SdlNativeFont::nativeFontDescent() const
{
    return 0;
}
    
int SdlNativeFont::nativeFontHeight() const
{
    return 0;
}
    
int SdlNativeFont::nativeFontLineSpacing() const
{
    return 0;
}
   
Rectanglei SdlNativeFont::nativeFontMeasure(const String &text) const
{
    return {};
}
    
int SdlNativeFont::nativeFontWidth(const String &text) const
{
    return 0;
}    

Image SdlNativeFont::nativeFontRasterize(const String &      text,
                                         const Image::Color &foreground,
                                         const Image::Color &background) const
{
    return {};
}
        
} // namespace de
