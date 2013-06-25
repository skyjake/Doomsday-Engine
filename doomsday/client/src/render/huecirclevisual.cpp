/** @file huecirclevisual.cpp HueCircle visualizer.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/mathutil.h> // M_HSVToRGB(), remove me (use QColor)

#include "de_base.h"
#include "de_graphics.h"

#include "HueCircle"

#include "render/huecirclevisual.h"

using namespace de;

void HueCircleVisual::draw(HueCircle &hueCircle, Vector3d const &viewOrigin,
                           Vector3f const &viewFrontVec) // static
{
    float const steps = 32;
    float const inner = 10;
    float const outer = 30;

    // Determine the origin of the circle in the map coordinate space.
    Vector3d center = hueCircle.origin(viewOrigin);

    // Draw the circle.
    glBegin(GL_QUAD_STRIP);
    for(int i = 0; i <= steps; ++i)
    {
        Vector3f off = hueCircle.offset(float(2 * de::PI) * i/steps);

        // Determine the RGB color for this angle.
        float color[3]; M_HSVToRGB(color, i/steps, 1, 1);

        glColor4f(color[0], color[1], color[2], .5f);
        glVertex3f(center.x + outer * off.x, center.y + outer * off.y, center.z + outer * off.z);

        // Saturation decreases toward the center.
        glColor4f(1, 1, 1, .15f);
        glVertex3f(center.x + inner * off.x, center.y + inner * off.y, center.z + inner * off.z);
    }
    glEnd();

    // Draw the current hue.
    float hue, saturation;
    Vector3f sel = hueCircle.colorAt(viewFrontVec, &hue, &saturation);

    Vector3f off = hueCircle.offset(float(2 * de::PI) * hue);

    glBegin(GL_LINES);
    if(saturation > 0)
    {
        glColor4f(sel.x, sel.y, sel.z, 1.f);
        glVertex3f(center.x + outer * off.x, center.y + outer * off.y, center.z + outer * off.z);
        glVertex3f(center.x + inner * off.x, center.y + inner * off.y, center.z + inner * off.z);
    }

    // Draw the edges.
    for(int i = 0; i < steps; ++i)
    {
        Vector3f off  = hueCircle.offset(float(2 * de::PI) * i/steps);
        Vector3f off2 = hueCircle.offset(float(2 * de::PI) * (i + 1)/steps);

        // Determine the RGB color for this angle.
        float color[3]; M_HSVToRGB(color, i/steps, 1, 1);

        glColor4f(color[0], color[1], color[2], 1.f);
        glVertex3f(center.x + outer * off.x,  center.y + outer * off.y,  center.z + outer * off.z);
        glVertex3f(center.x + outer * off2.x, center.y + outer * off2.y, center.z + outer * off2.z);

        // Saturation decreases in the center.
        float alpha = (de::fequal(saturation, 0)? 0
                       : 1 - de::abs(M_CycleIntoRange(hue - i/steps + .5f, 1) - .5f) * 2.5f);

        glColor4f(sel.x, sel.y, sel.z, alpha);
        float s = inner + (outer - inner) * saturation;
        glVertex3f(center.x + s * off.x,  center.y + s * off.y,  center.z + s * off.z);
        glVertex3f(center.x + s * off2.x, center.y + s * off2.y, center.z + s * off2.z);
    }
    glEnd();
}
