/*
 * The Doomsday Engine Project
 * Common OpenGL Shaders: HSV/RGB Color Conversions
 *
 * Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * Converts an RGBA value to HSVA. Hue uses degrees as the unit.
 */
highp vec4 rgbToHsv(highp vec4 rgb)
{
    highp vec4 hsv;
    hsv.a = rgb.a;    

    highp float rgbMin = min(min(rgb.r, rgb.g), rgb.b);
    highp float rgbMax = max(max(rgb.r, rgb.g), rgb.b); 
    highp float delta  = rgbMax - rgbMin;

    hsv.z = rgbMax;

    if (delta < 0.00001)
    {
        hsv.xy = vec2(0.0);
        return hsv;
    }

    if (rgbMax > 0.0)
    {
        hsv.y = delta / rgbMax;
    }
    else 
    {
        hsv.xy = vec2(0.0);
        return hsv;
    }
        
    if (rgb.r >= rgbMax)
    {
        hsv.x = (rgb.g - rgb.b) / delta;
    }
    else
    {
        if (rgb.g >= rgbMax)
        {
            hsv.x = 2.0 + (rgb.b - rgb.r) / delta;
        }
        else
        {
            hsv.x = 4.0 + (rgb.r - rgb.g) / delta;
        }
    }

    hsv.x *= 60.0;
    if (hsv.x < 0.0)
    {
        hsv.x += 360.0;
    }
    return hsv;
}

highp vec4 hsvToRgb(highp vec4 hsv)
{
    highp vec4 rgb;
    rgb.a = hsv.a;

    if (hsv.y <= 0.0) 
    {
       rgb.rgb = vec3(hsv.z);
       return rgb;
    }
    
    highp float hh = hsv.x;
    if (hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    
    highp float ff = fract(hh); 
    highp float p = hsv.z * (1.0 -  hsv.y);
    highp float q = hsv.z * (1.0 - (hsv.y *        ff));
    highp float t = hsv.z * (1.0 - (hsv.y * (1.0 - ff)));

    int i = int(hh);

    if (i == 0)
    {
        rgb.rgb = vec3(hsv.z, t, p);
    }
    else if (i == 1)
    {
        rgb.rgb = vec3(q, hsv.z, p);
    }
    else if (i == 2)
    {
        rgb.rgb = vec3(p, hsv.z, t);
    }
    else if (i == 3)
    {
        rgb.rgb = vec3(p, q, hsv.z);
    }
    else if (i == 4)
    {
        rgb.rgb = vec3(t, p, hsv.z);
    }
    else 
    {
        rgb.rgb = vec3(hsv.z, p, q);
    }

    return rgb;
}
