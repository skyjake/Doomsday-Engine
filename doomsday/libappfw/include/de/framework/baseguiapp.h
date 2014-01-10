/** @file baseguiapp.h  Base class for GUI applications.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_BASEGUIAPP_H
#define LIBAPPFW_BASEGUIAPP_H

#include "libappfw.h"

#include <de/GuiApp>
#include <de/GLShaderBank>

namespace de {

/**
 * Base class for GUI applications.
 *
 * Contains all the shared resources and other data that is needed by the UI framework.
 */
class LIBAPPFW_PUBLIC BaseGuiApp : public GuiApp
{
public:
    BaseGuiApp(int &argc, char **argv);

public:
    static BaseGuiApp &app();
    static GLShaderBank &shaders();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BASEGUIAPP_H
