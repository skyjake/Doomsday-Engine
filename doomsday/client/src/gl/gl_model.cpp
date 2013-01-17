/** @file gl_model.cpp 3D Model Renderable.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cstring>

#include <de/Error>

#include "de_platform.h"
#include "de_graphics.h"

#include "gl/gl_model.h"

using namespace de;

void model_frame_t::getBounds(float _min[3], float _max[3]) const
{
    std::memcpy(_min, min, sizeof(float) * 3);
    std::memcpy(_max, max, sizeof(float) * 3);
}

float model_frame_t::horizontalRange(float *top, float *bottom) const
{
    *top    = max[VY];
    *bottom = min[VY];
    return max[VY] - min[VY];
}

bool model_t::validFrameNumber(int value) const
{
    return (value >= 0 && value < info.numFrames);
}

model_frame_t &model_t::frame(int number) const
{
    if(!validFrameNumber(number))
        throw Error("model_t::frame", QString("Invalid frame number %i. Valid range is [0..%i)")
                                          .arg(number).arg(info.numFrames));
    return frames[number];
}

int model_t::frameNumForName(char const *name) const
{
    if(name && name[0])
    {
        for(int i = 0; i < info.numFrames; ++i)
        {
            if(!qstricmp(frames[i].name, name))
                return i;
        }
    }
    return 0;
}
