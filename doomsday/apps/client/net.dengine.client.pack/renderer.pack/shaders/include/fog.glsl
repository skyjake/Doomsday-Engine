/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: Fog (fragment shader)
 *
 * Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

uniform highp vec4 uFogRange; // startDepth, fogDepth, nearclip, farclip
uniform highp vec4 uFogColor; // set alpha to zero to disable fog

void applyFog()
{
    if(uFogColor.a > 0.0)
    {
        highp float near = uFogRange.z;
        highp float far  = uFogRange.w;
        
        // First convert the fragment Z back to view space.
        highp float zNorm = gl_FragCoord.z * 2.0 - 1.0;
        highp float zEye = -2.0 * far * near / (zNorm * (far - near) - (far + near));
        
        highp float fogAmount = clamp((zEye - uFogRange.x) / uFogRange.y, 0.0, 1.0);
        gl_FragColor.rgb = mix(gl_FragColor.rgb, uFogColor.rgb, fogAmount);
        
//        gl_FragColor.rgb = vec3(zEye, 0.0, normZ);
    }    
}
