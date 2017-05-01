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

#version 330

uniform int uTexEnabled;
uniform int uTexMode;
uniform vec4 uTexModeColor;
uniform float uAlphaLimit;
uniform sampler2D uTex0;
uniform sampler2D uTex1;

in vec4 vColor;
in vec2 vTexCoord[2];

void main()
{
    out_FragColor = vColor;
    
    // Colors from textures.    
    vec4 texColor[2] = vec4[2] (vec4(1.0), vec4(1.0));
    if ((uTexEnabled & 0x1) != 0) {
        texColor[0] = texture(uTex0, vTexCoord[0]);
    }
    if ((uTexEnabled & 0x2) != 0) {
        texColor[1] = texture(uTex1, vTexCoord[1]);
    }
    
    // Modulate the texture colors in the requested manner.
    switch (uTexMode) {
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
        out_FragColor.rgb *= mix(texColor[0].rgb, texColor[1].rgb, uTexModeColor.a);      
        break;
    case 3:
        // Texture interpolation.
        out_FragColor.rgb = mix(texColor[0].rgb, texColor[1].rgb, uTexModeColor.a);
        break;
    case 4:
        // Sector light, dynamic light, and texture.
        out_FragColor.rgb += texColor[0].a * uTexModeColor.rgb;
        out_FragColor     *= texColor[1];
        break;
    case 6:
        // Simple dynlight addition (add to primary color).
        out_FragColor.rgb += texColor[0].a * uTexModeColor.rgb;
        break;
    case 8:
        // Texture and Detail.
        out_FragColor     *= texColor[0];
        out_FragColor.rgb *= texColor[1].rgb * 2.0;
        break;
    case 10:
        // Sector light * texture + dynamic light.
        out_FragColor     *= texColor[0];
        out_FragColor.rgb += texColor[1].rgb * uTexModeColor.rgb;
        break;
    case 11:
        // Normal modulation, alpha of 2nd stage.
        // Tex0: texture
        // Tex1: shiny texture
        out_FragColor.rgb *= texColor[1].rgb;
        out_FragColor.a   *= texColor[0].a;
        break;
    }
    
    // Alpha test.
    if (out_FragColor.a <= uAlphaLimit) {
        discard;
    }
    
    applyFog();
}
