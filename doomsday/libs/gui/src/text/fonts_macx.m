/** @file fonts_macx.m  System font routines for macOS.
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

#import <AppKit/NSFont.h>
#import <AppKit/NSFontManager.h>

static NSFontWeight fontWeight(int weight)
{
    return (weight == 0   ? NSFontWeightUltraLight :
            weight == 25  ? NSFontWeightLight :
            weight == 75  ? NSFontWeightBold :
            weight == 100 ? NSFontWeightBlack :
                            NSFontWeightRegular);
}

static NSFont *applyStyle(NSFont *font, int monospace, int italic)
{
    if (italic || monospace)
    {
        NSFontTraitMask style = (monospace ? NSFixedPitchFontMask : 0) |
                                (italic    ? NSItalicFontMask     : 0);
        NSFont *conv = [[NSFontManager sharedFontManager] convertFont:font
                                                          toHaveTrait:style];
        if (conv != font)
        {
            [font release];
            return conv;
        }
    }
    return font;
}

void *Apple_CreateSystemFont(float size, int weight, int italic)
{
    // Just ask for .NSSFUIText and .NSSFUIDisplay
    return applyStyle([NSFont systemFontOfSize:size weight:fontWeight(weight)], 0, italic);
}

void *Apple_CreateMonospaceSystemFont(float size, int weight, int italic)
{
    return applyStyle([NSFont systemFontOfSize:size weight:fontWeight(weight)], 1, italic);
}
