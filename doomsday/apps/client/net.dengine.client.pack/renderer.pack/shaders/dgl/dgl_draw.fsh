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

vec4 sampleTexture0(int batchIndex, vec2 uv)
{
    switch (batchIndex)
    {
        case 0: return texture(uTex0[0], uv);
        case 1: return texture(uTex0[1], uv);
        case 2: return texture(uTex0[2], uv);
        case 3: return texture(uTex0[3], uv);
        case 4: return texture(uTex0[4], uv);
        case 5: return texture(uTex0[5], uv);
        case 6: return texture(uTex0[6], uv);
        case 7: return texture(uTex0[7], uv);
#if DGL_BATCH_MAX > 8
        case 8: return texture(uTex0[8], uv);
#endif
#if DGL_BATCH_MAX > 9
        case 9: return texture(uTex0[9], uv);
#endif
#if DGL_BATCH_MAX > 10
        case 10: return texture(uTex0[10], uv);
#endif
#if DGL_BATCH_MAX > 11
        case 11: return texture(uTex0[11], uv);
#endif
#if DGL_BATCH_MAX > 12
        case 12: return texture(uTex0[12], uv);
#endif
#if DGL_BATCH_MAX > 13
        case 13: return texture(uTex0[13], uv);
#endif
#if DGL_BATCH_MAX > 14
        case 14: return texture(uTex0[14], uv);
#endif
#if DGL_BATCH_MAX > 15
        case 15: return texture(uTex0[15], uv);
#endif
    }
}

vec4 sampleTexture1(int batchIndex, vec2 uv)
{
    switch (batchIndex)
    {
        case 0: return texture(uTex1[0], uv);
        case 1: return texture(uTex1[1], uv);
        case 2: return texture(uTex1[2], uv);
        case 3: return texture(uTex1[3], uv);
        case 4: return texture(uTex1[4], uv);
        case 5: return texture(uTex1[5], uv);
        case 6: return texture(uTex1[6], uv);
        case 7: return texture(uTex1[7], uv);
#if DGL_BATCH_MAX > 8
        case 8: return texture(uTex1[8], uv);
#endif
#if DGL_BATCH_MAX > 9
        case 9: return texture(uTex1[9], uv);
#endif
#if DGL_BATCH_MAX > 10
        case 10: return texture(uTex1[10], uv);
#endif
#if DGL_BATCH_MAX > 11
        case 11: return texture(uTex1[11], uv);
#endif
#if DGL_BATCH_MAX > 12
        case 12: return texture(uTex1[12], uv);
#endif
#if DGL_BATCH_MAX > 13
        case 13: return texture(uTex1[13], uv);
#endif
#if DGL_BATCH_MAX > 14
        case 14: return texture(uTex1[14], uv);
#endif
#if DGL_BATCH_MAX > 15
        case 15: return texture(uTex1[15], uv);
#endif
    }
}
void main()
{
    out_FragColor = vColor;

    // Colors from textures.
    vec4 texColor[2] = vec4[2](vec4(1.0), vec4(1.0));
    if ((uTexEnabled[vBatchIndex] & 0x1) != 0)
    {
        texColor[0] = sampleTexture0(vBatchIndex, vTexCoord[0]);
    }
    if ((uTexEnabled[vBatchIndex] & 0x2) != 0)
    {
        texColor[1] = sampleTexture1(vBatchIndex, vTexCoord[1]);
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
