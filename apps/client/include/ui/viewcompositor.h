/** @file viewcompositor.h  Game view compositor.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UI_VIEWCOMPOSITOR_H
#define DE_CLIENT_UI_VIEWCOMPOSITOR_H

#include <de/gltextureframebuffer.h>

class PostProcessing;

/**
 * Compositor for the game view.
 *
 * Owns a framebuffer for the player view and manages the compositing of the various
 * view layers.
 *
 * The game view itself is stored in a texture, sized according to the view window
 * (which may be scaled down for vanilla emulation) and renderer pixel density. I.e.,
 * the view border is outside this view texture.
 *
 * Lens FX are rendered inside the player view framebuffer after 3D rendering has been
 * completed and the color and depth textures are available.
 *
 * The finished game view texture is kept around until the next frame begins. It can be
 * copied for savegames, etc. at any time.
 *
 * After Lens FX, the contents of the framebuffer are ready for compositing with
 * additional layers, such as the view border and game HUD. These are drawn into the
 * current framebuffer.
 */
class ViewCompositor
{
public:
    ViewCompositor();

    void setPlayerNumber(int playerNum);

    /**
     * Release all GL resources.
     */
    void glDeinit();

    /**
     * Renders the contents of the game view framebuffer of a player. All enabled Lens FX
     * are rendered after the @a renderFunc callback has finished.
     *
     * The framebuffer is available via gameView() at any time.
     *
     * @param playerNum   Player number.
     * @param renderFunc  Callback that does the actual rendering.
     */
    void renderGameView(std::function<void (int)> renderFunc);

    de::GLTextureFramebuffer       & gameView();
    const de::GLTextureFramebuffer & gameView() const;

    /**
     * Draws the game view and additional view layers into the current render target,
     * using the current GL viewport.
     *
     * Can be called at any time.
     *
     * Note that the existing contents of the game view framebuffer are used as-is; the
     * game view needs to be redrawn separately beforehand, if needed.
     */
    void drawCompositedLayers();

    PostProcessing &postProcessing();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_VIEWCOMPOSITOR_H
