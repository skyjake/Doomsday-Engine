/** @file preferences.h  Widget for user preferences.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <de/dialogwidget.h>
#include <de/nativepath.h>

class Preferences : public de::DialogWidget
{
public:
    explicit Preferences();
    
    static de::NativePath iwadFolder();
    static bool isIwadFolderRecursive();
//    static QFont consoleFont();

//signals:
//    void consoleFontChanged();

public:
    void saveState();
    void validate();
//    void selectFont();
    
private:
    DE_PRIVATE(d)
};

#endif // PREFERENCES_H
