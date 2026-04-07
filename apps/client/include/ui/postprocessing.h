/** @file postprocessing.h  World frame post processing.
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

#ifndef DE_CLIENT_FX_POSTPROCESSING_H
#define DE_CLIENT_FX_POSTPROCESSING_H

#include "render/consoleeffect.h"
#include <de/matrix.h>
#include <de/time.h>
#include <de/gltexture.h>

/**
 * Post-processing of rendered camera lens frames. Maintains an offscreen
 * render target and provides a way to draw it back to the regular target
 * with shader effects applied.
 */
class PostProcessing
{
public:
    PostProcessing();

    /**
     * Determines whether the effect is active. If it isn't, it can be skipped
     * altogether when post processing a frame.
     */
    bool isActive() const;

    /**
     * Fades in, or immediately takes into use, a new post-processing shader.
     * Only shaders in the "fx.post" namespace can be used.
     *
     * If a shader is already in use, it will simply be swapped immediately
     * with the new shader rather than crossfaded.
     *
     * @param fxPostShader  Name of the shader under "fx.post".
     * @param span          Duration of the fade in animation.
     */
    void fadeInShader(const de::String &fxPostShader, de::TimeSpan span);

    void fadeOut(de::TimeSpan span);

    /**
     * Sets a constant opacity factor that is applied in addition to the fade.
     *
     * @param opacity  Opacity factor. 1.0 by default.
     */
    void setOpacity(float opacity);

    void glInit();
    void glDeinit();
    void update();
    void draw(const de::Mat4f &mvpMatrix, const de::GLTexture &frame);

public:
    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_FX_POSTPROCESSING_H
