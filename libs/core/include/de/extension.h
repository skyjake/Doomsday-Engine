/** @file extension.h  Binary extension components.
 *
 * An extension is a static library that registers extern C entry points.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_EXTENSION_H
#define LIBCORE_EXTENSION_H

#include "de/libcore.h"
#include "de/string.h"

namespace de {

/**
 * Registers an available extension and a function for retrieving its entry points.
 *
 * @param name            Name of the extension.
 * @param getProcAddress  Function that is given the name of an entry point.
 *                        Returns the function address.
 */
DE_PUBLIC void registerExtension(const char *name, void *(*getProcAddress)(const char *));

#define DE_EXTENSION(Name) \
    DE_EXTERN_C void *extension_##Name##_symbol(const char *); \
    struct Extension_##Name { \
        Extension_##Name() { \
            de::registerExtension(#Name, extension_##Name##_symbol); \
        } \
    }; \
    static Extension_##Name extension_registrar_##Name

#define DE_SYMBOL_PTR(var, symbolName) \
    if (!iCmpStr(var, #symbolName)) { \
        return de::function_cast<void *>(symbolName); \
    }

#define DE_EXT_SYMBOL_PTR(ext, var, symbolName) \
    if (!iCmpStr(var, #symbolName)) { \
        return de::function_cast<void *>(ext##_##symbolName); \
    }

DE_PUBLIC bool isExtensionRegistered(const char *name);

DE_PUBLIC StringList extensions();

DE_PUBLIC void *extensionSymbol(const char *extensionName, const char *symbolName);

} // namespace de

#endif // LIBCORE_EXTENSION_H
