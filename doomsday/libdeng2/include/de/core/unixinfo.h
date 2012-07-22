/**
 * @file unixinfo.h
 * Unix system-level configuration. @ingroup core
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG2_UNIXINFO_H
#define LIBDENG2_UNIXINFO_H

#include "../String"

namespace de {

/**
 * System-level configuration preferences for the Unix platform. These are used
 * for setting specific directory locations, e.g., where shared libraries are
 * expected to be found. The configuration has two levels: system-global
 * configuration under <tt>/etc</tt> and user-specific configuration under
 * <tt>~/.doomsday</tt>.
 *
 * On non-Unix platforms, UnixInfo is instantiated normally but no input files
 * are parsed.
 */
class UnixInfo
{
public:
    /**
     * Loads the system-level Info files.
     */
    UnixInfo();

    ~UnixInfo();

    /**
     * Returns a path preference.
     *
     * @param key    Path identifier.
     * @param value  The value is returned here.
     *
     * @return  Path.
     */
    bool path(const String& key, String& value) const;

private:
    struct Instance;
    Instance* d;
};

} // namespace de

#endif // LIBDENG2_UNIXINFO_H
