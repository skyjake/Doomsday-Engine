/** @file rendererappearanceeditor.h
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_RENDERERAPPEARANCEEDITOR_H
#define DE_CLIENT_RENDERERAPPEARANCEEDITOR_H

#include <de/ipersistent.h>
#include "ui/widgets/sidebarwidget.h"

/**
 * Editor for modifying the settings for the renderer's visual appearance.
 *
 * Automatically installs itself into the main window's right sidebar.
 *
 * @see ClientApp::rendererAppearanceSettings()
 */
class RendererAppearanceEditor : public SidebarWidget
                               , public de::IPersistent
{
public:
    RendererAppearanceEditor();

    void operator >> (de::PersistentState &toState) const;
    void operator << (const de::PersistentState &fromState);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_RENDERERAPPEARANCEEDITOR_H
