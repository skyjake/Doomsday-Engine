/** @file baseguiapp.h  Base class for GUI applications.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_BASEGUIAPP_H
#define LIBAPPFW_BASEGUIAPP_H

#include "../libappfw.h"
#include "../PersistentState"

#include <de/GuiApp>
#include <de/GLShaderBank>

/**
 * Macro for conveniently accessing the de::BaseGuiApp singleton instance.
 */
#define DENG2_BASE_GUI_APP   (static_cast<de::BaseGuiApp *>(qApp))

namespace de {

class VRConfig;

/**
 * Base class for GUI applications.
 *
 * Contains all the shared resources and other data that is needed by the UI framework.
 */
class LIBAPPFW_PUBLIC BaseGuiApp : public GuiApp
{
public:
    BaseGuiApp(int &argc, char **argv);

    void initSubsystems(SubsystemInitFlags flags = DefaultSubsystems);

public:
    static BaseGuiApp &app();
    static PersistentState &persistentUIState();
    static GLShaderBank &shaders();
    static VRConfig &vr();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BASEGUIAPP_H
