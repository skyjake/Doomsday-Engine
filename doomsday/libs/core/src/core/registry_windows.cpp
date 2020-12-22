/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/core/registry.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace de {

String WindowsRegistry::textValue(const String &key, const String &name)
{
    String value;
    void *data = nullptr;
    DWORD size = 0;

    while (!data)        
    {
        if (size)
        {
            data = malloc(size);
        }
        if (RegGetValue(key.beginsWith("HKEY_CURRENT_USER\\") ?
                        HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
                        key, 
                        name,
                        RRF_RT_REG_SZ,
                        NULL,
                        data,
                        &size) != ERROR_SUCCESS)
        {
            break;
        }
        if (data)
        {
            value = String(reinterpret_cast<const char *>(data));
        }
    }

    return value;
}
    
} // namespace de
