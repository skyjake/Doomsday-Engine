/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Legacy DGL Drawing
 *
 * Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../include/fog.glsl"

uniform int uTexEnabled[DGL_BATCH_MAX];
uniform int uTexMode[DGL_BATCH_MAX];
uniform vec4  uTexModeColor[DGL_BATCH_MAX];
uniform float uAlphaLimit[DGL_BATCH_MAX];
uniform sampler2D uTex0[DGL_BATCH_MAX];
uniform sampler2D uTex1[DGL_BATCH_MAX];

flat DE_VAR int vBatchIndex;
DE_VAR vec4 vColor;
DE_VAR vec2 vTexCoord[2];

#define SAMPLE_UTEX0(n)     if (vBatchIndex == n) { return texture(uTex0[n], uv); }
#define SAMPLE_UTEX1(n)     if (vBatchIndex == n) { return texture(uTex1[n], uv); }

vec4 sampleTexture0(vec2 uv)
{
    SAMPLE_UTEX0(0)
    SAMPLE_UTEX0(1)
    SAMPLE_UTEX0(2)
    SAMPLE_UTEX0(3)
#if DGL_BATCH_MAX > 4
    SAMPLE_UTEX0(4)
    SAMPLE_UTEX0(5)
    SAMPLE_UTEX0(6)
    SAMPLE_UTEX0(7)
#endif
#if DGL_BATCH_MAX > 8
    SAMPLE_UTEX0(8)
#endif
#if DGL_BATCH_MAX > 9
    SAMPLE_UTEX0(9)
#endif
#if DGL_BATCH_MAX > 10
    SAMPLE_UTEX0(10)
#endif
#if DGL_BATCH_MAX > 11
    SAMPLE_UTEX0(11)
#endif
#if DGL_BATCH_MAX > 12
    SAMPLE_UTEX0(12)
#endif
#if DGL_BATCH_MAX > 13
    SAMPLE_UTEX0(13)
#endif
#if DGL_BATCH_MAX > 14
    SAMPLE_UTEX0(14)
#endif
#if DGL_BATCH_MAX > 15
    SAMPLE_UTEX0(15)
#endif
    return vec4(1.0, 0.0, 1.0, 1.0);
}

vec4 sampleTexture1(vec2 uv)
{
    SAMPLE_UTEX1(0)
    SAMPLE_UTEX1(1)
    SAMPLE_UTEX1(2)
    SAMPLE_UTEX1(3)
#if DGL_BATCH_MAX > 4
    SAMPLE_UTEX1(4)
    SAMPLE_UTEX1(5)
    SAMPLE_UTEX1(6)
    SAMPLE_UTEX1(7)
#endif
#if DGL_BATCH_MAX > 8
    SAMPLE_UTEX1(8)
#endif
#if DGL_BATCH_MAX > 9
    SAMPLE_UTEX1(9)
#endif
#if DGL_BATCH_MAX > 10
    SAMPLE_UTEX1(10)
#endif
#if DGL_BATCH_MAX > 11
    SAMPLE_UTEX1(11)
#endif
#if DGL_BATCH_MAX > 12
    SAMPLE_UTEX1(12)
#endif
#if DGL_BATCH_MAX > 13
    SAMPLE_UTEX1(13)
#endif
#if DGL_BATCH_MAX > 14
    SAMPLE_UTEX1(14)
#endif
#if DGL_BATCH_MAX > 15
    SAMPLE_UTEX1(15)
#endif
    return vec4(1.0, 0.0, 1.0, 1.0);
}

void main()
{
    out_FragColor = vColor;

    // Colors from textures.
    vec4 texColor[2] = vec4[2](vec4(1.0), vec4(1.0));
    if ((uTexEnabled[vBatchIndex] & 0x1) != 0)
    {
        texColor[0] = sampleTexture0(vTexCoord[0]);
    }
    if ((uTexEnabled[vBatchIndex] & 0x2) != 0)
    {
        texColor[1] = sampleTexture1(vTexCoord[1]);
    }

    // Modulate the texture colors in the requested manner.
    switch (uTexMode[vBatchIndex])
    {
        case 0:
            // No modulation: just replace with texture.
            out_FragColor = texColor[0];
            break;
        case 1:
            // Normal texture modulation with primary color.
            out_FragColor *= texColor[0];
            break;
        case 2:
            // Texture interpolation and modulation with primary color.
            out_FragColor.rgb *=
                mix(texColor[0].rgb, texColor[1].rgb, uTexModeColor[vBatchIndex].a);
            break;
        case 3:
            // Texture interpolation.
            out_FragColor.rgb = mix(texColor[0].rgb, texColor[1].rgb, uTexModeColor[vBatchIndex].a);
            break;
        case 4:
            // Sector light, dynamic light, and texture.
            out_FragColor.rgb += texColor[0].a * uTexModeColor[vBatchIndex].rgb;
            out_FragColor *= texColor[1];
            break;
        case 5:
            // DM_TEXTURE_PLUS_LIGHT
            out_FragColor *= texColor[0];
            out_FragColor.rgb += texColor[1].a * uTexModeColor[vBatchIndex].rgb;
            break;
        case 6:
            // Simple dynlight addition (add to primary color).
            out_FragColor.rgb += texColor[0].a * uTexModeColor[vBatchIndex].rgb;
            break;
        case 7:
            // Dynlight without primary color.
            out_FragColor.rgb = texColor[0].a * uTexModeColor[vBatchIndex].rgb;
            break;
        case 8:
            // Texture and Detail.
            out_FragColor *= texColor[0];
            out_FragColor.rgb *= texColor[1].rgb * 2.0;
            break;
        case 10:
            // Sector light * texture + dynamic light.
            out_FragColor *= texColor[0];
            out_FragColor.rgb += texColor[1].rgb * uTexModeColor[vBatchIndex].rgb;
            break;
        case 11:
            // Normal modulation, alpha of 2nd stage.
            // Tex0: texture
            // Tex1: shiny texture
            out_FragColor.rgb *= texColor[1].rgb;
            out_FragColor.a *= texColor[0].a;
            break;
    }

    // Alpha test.
    if (out_FragColor.a <= uAlphaLimit[vBatchIndex])
    {
        discard;
    }

    applyFog();
}
