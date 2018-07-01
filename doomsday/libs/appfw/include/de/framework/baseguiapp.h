/** @file baseguiapp.h  Base class for GUI applications.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/Rule>
#include <de/WaveformBank>

/**
 * Macro for conveniently accessing the de::BaseGuiApp singleton instance.
 */
#define DE_BASE_GUI_APP   (static_cast<de::BaseGuiApp *>(DE_APP))

namespace de {

class VRConfig;

/**
 * Base class for GUI applications.
 *
 * Contains all the shared resources and other data that is needed by the UI framework.
 *
 * @ingroup appfw
 */
class LIBAPPFW_PUBLIC BaseGuiApp : public GuiApp
{
public:
    BaseGuiApp(const StringList &args);

    virtual void glDeinit();

    void initSubsystems(SubsystemInitFlags flags = DefaultSubsystems);

    /**
     * The ratio of pixels per point. For example, this is 2.0 if there are two pixels per point.
     */
    const Rule &pixelRatio() const;

    /**
     * Sets a new pixel ratio. This replaces the initial automatically detected pixel ratio.
     *
     * @param pixelRatio  Pixel ratio.
     */
    void setPixelRatio(float pixelRatio);
    
    /**
     * Enters the "native UI" mode that temporarily switches the main window to a
     * regular window and restores the desktop display mode. This allows the user to
     * access native UI widgets normally.
     *
     * Call this before showing native UI widgets. You must call endNativeUIMode()
     * afterwards.
     */
    void beginNativeUIMode();

    /**
     * Ends the "native UI" mode, restoring the previous main window properties.
     */
    void endNativeUIMode();

public:
    static BaseGuiApp &     app();
    static PersistentState &persistentUIState();
    static GLShaderBank &   shaders();
    static WaveformBank &   waveforms();
    static VRConfig &       vr();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BASEGUIAPP_H
