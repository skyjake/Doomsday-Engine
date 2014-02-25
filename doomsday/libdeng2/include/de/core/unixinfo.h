/**
 * @file unixinfo.h
 * Unix system-level configuration.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_UNIXINFO_H
#define LIBDENG2_UNIXINFO_H

#include "../String"
#include "../NativePath"

namespace de {

/**
 * System-level configuration preferences for the Unix platform. These are used
 * for setting specific directory locations, e.g., where shared libraries are
 * expected to be found. The configuration has two levels: system-global
 * configuration under <tt>/etc</tt> and user-specific configuration under
 * <tt>~/.doomsday</tt>.
 *
 * UnixInfo exists because hand-edited config files are commonly found on Unix
 * but not on the other platforms. On non-Unix platforms, UnixInfo is
 * instantiated normally but no input files are parsed. There are equivalent
 * mechanisms on these platforms (on Windows, the closest is the registry; on
 * Mac OS X, ~/Library/Preferences/) but these are not directly used by
 * libdeng2. Instead of these, one should use Config (or QSettings) for
 * platform-independent persistent configuration.
 *
 * @ingroup core
 */
class UnixInfo
{
public:
    /**
     * Loads the system-level Info files.
     */
    UnixInfo();

    /**
     * Returns a path preference.
     *
     * @param key    Path identifier.
     * @param value  The value is returned here.
     *
     * @return  @c true, if the path was defined; otherwise @c false.
     */
    bool path(String const &key, NativePath &value) const;

    /**
     * Returns a defaults preference.
     *
     * @param key    Path identifier.
     * @param value  The value is returned here.
     *
     * @return @c true, iff the value is defined in the 'defaults' info file.
     */
    bool defaults(String const &key, String &value) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_UNIXINFO_H
