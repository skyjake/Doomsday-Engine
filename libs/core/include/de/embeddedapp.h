/** @file embeddedapp.h  App singleton to be used within another application.
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

#ifndef LIBCORE_EMBEDDEDAPP_H
#define LIBCORE_EMBEDDEDAPP_H

#include "de/app.h"
#include "de/eventloop.h"
#include "de/loop.h"

namespace de {

/**
 * App singleton to be used within another application.
 * @ingroup core
 */
class DE_PUBLIC EmbeddedApp : public App
{
public:
    EmbeddedApp(const StringList &args);

    NativePath appDataPath() const override;

    void processEvents();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_EMBEDDEDAPP_H
