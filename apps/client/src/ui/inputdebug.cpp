/** @file inputdebug.cpp  Input debug visualizer.
 *
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h" // strdup macro
#include "ui/inputdebug.h"

#ifdef DE_DEBUG

#include <de/legacy/concurrency.h>
#include <de/legacy/ddstring.h>
#include <de/legacy/point.h>
#include <de/legacy/timer.h> // SECONDSPERTIC
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <de/keyevent.h>
#include <de/glinfo.h>
#include "clientapp.h"
#include "dd_def.h"
#include "dd_main.h"
#include "sys_system.h" // novideo

#include "gl/gl_main.h"
#include "gl/gl_draw.h"

#include "api_fontrender.h"

#include "ui/b_main.h"
#include "ui/b_util.h"
#include "ui/clientwindow.h"
#include "ui/joystick.h"
#include "ui/infine/finale.h"
#include "ui/inputsystem.h"
#include "ui/inputdevice.h"
#include "ui/axisinputcontrol.h"
#include "ui/buttoninputcontrol.h"
#include "ui/hatinputcontrol.h"
#include "ui/sys_input.h"
#include "ui/ui_main.h"

using namespace de;

static byte devRendJoyState;
static byte devRendKeyState;
static byte devRendMouseState;

#define MAX_KEYMAPPINGS  256
static dbyte shiftKeyMappings[MAX_KEYMAPPINGS];

// Initialize key mapping table.
static void initKeyMappingsOnce()
{
    // Already been here?
    if (shiftKeyMappings[1] == 1) return;

    dbyte defaultShiftTable[96] = // Contains characters 32 to 127.
    {
    /* 32 */    ' ', 0, 0, 0, 0, 0, 0, '"',
    /* 40 */    0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
    /* 50 */    '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
    /* 60 */    0, '+', 0, 0, 0, 'a', 'b', 'c', 'd', 'e',
    /* 70 */    'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    /* 80 */    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    /* 90 */    'z', '{', '|', '}', 0, 0, '~', 'A', 'B', 'C',
    /* 100 */   'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    /* 110 */   'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    /* 120 */   'X', 'Y', 'Z', 0, 0, 0, 0, 0
    };

    /// @todo does not belong at this level.
    for (int i = 0; i < 256; ++i)
    {
        if (i >= 32 && i <= 127)
            shiftKeyMappings[i] = defaultShiftTable[i - 32] ? defaultShiftTable[i - 32] : i;
        else
            shiftKeyMappings[i] = i;
    }
}

static void initDrawStateForVisual(const Point2Raw *origin)
{
    FR_PushAttrib();

    // Ignore zero offsets.
    if (origin && !(origin->x == 0 && origin->y == 0))
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(origin->x, origin->y, 0);
    }
}

static void endDrawStateForVisual(const Point2Raw *origin)
{
    // Ignore zero offsets.
    if (origin && !(origin->x == 0 && origin->y == 0))
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

    FR_PopAttrib();
}

/**
 * Apply all active modifiers to the key.
 */
static dbyte modKey(int ddkey)
{
    extern bool shiftDown;

    initKeyMappingsOnce();

    if (shiftDown)
    {
        DE_ASSERT(ddkey >= 0 && ddkey < MAX_KEYMAPPINGS);
        ddkey = shiftKeyMappings[ddkey];
    }

    if (ddkey >= DDKEY_NUMPAD7 && ddkey <= DDKEY_NUMPAD0)
    {
        static dbyte const numPadKeys[10] = { '7', '8', '9', '4', '5', '6', '1', '2', '3', '0' };
        return numPadKeys[ddkey - DDKEY_NUMPAD7];
    }
    else if (ddkey == DDKEY_DIVIDE)
    {
        return '/';
    }
    else if (ddkey == DDKEY_SUBTRACT)
    {
        return '-';
    }
    else if (ddkey == DDKEY_ADD)
    {
        return '+';
    }
    else if (ddkey == DDKEY_DECIMAL)
    {
        return '.';
    }
    else if (ddkey == DDKEY_MULTIPLY)
    {
        return '*';
    }

    return dbyte(ddkey);
}

void Rend_RenderButtonStateVisual(InputDevice &device, int buttonID, const Point2Raw *_origin,
    RectRaw *geometry)
{
#define BORDER 4

    float const upColor[] = { .3f, .3f, .3f, .6f };
    float const downColor[] = { .3f, .3f, 1, .6f };
    float const expiredMarkColor[] = { 1, 0, 0, 1 };
    float const triggeredMarkColor[] = { 1, 0, 1, 1 };

    if (geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    const ButtonInputControl &button = device.button(buttonID);

    Point2Raw origin;
    origin.x = _origin? _origin->x : 0;
    origin.y = _origin? _origin->y : 0;

    // Compose the label.
    String label;
    if (!button.name().isEmpty())
    {
        // Use the symbolic name.
        label = button.name();
    }
    else if (&device == ClientApp::input().devicePtr(IDEV_KEYBOARD))
    {
        // Perhaps a printable ASCII character?
        // Apply all active modifiers to the key.
        dbyte asciiCode = modKey(buttonID);
        if (asciiCode > 32 && asciiCode < 127)
        {
            label = String::asText(asciiCode);
        }

        // Is there symbolic name in the bindings system?
        if (label.isEmpty())
        {
            label = B_ShortNameForKey(buttonID, false/*do not force lowercase*/);
        }
    }

    if (label.isEmpty())
    {
        label = Stringf("#%03i", buttonID);
    }

    initDrawStateForVisual(&origin);

    // Calculate the size of the visual according to the dimensions of the text.
    Size2Raw textSize;
    FR_TextSize(&textSize, label);

    // Enlarge by BORDER pixels.
    Rectanglei textGeom = Rectanglei::fromSize(Vec2i(0, 0),
                                               Vec2ui(textSize.width  + BORDER * 2,
                                                         textSize.height + BORDER * 2));

    // Draw a background.
    DGL_Color4fv(button.isDown()? downColor : upColor);
    GL_DrawRect(textGeom);

    // Draw the text.
    DGL_Enable(DGL_TEXTURE_2D);
    const Point2Raw textOffset = {{{BORDER, BORDER}}};
    FR_DrawText(label, &textOffset);
    DGL_Disable(DGL_TEXTURE_2D);

    // Mark expired?
    if (button.bindContextAssociation() & InputControl::Expired)
    {
        const int markSize = .5f + de::min(textGeom.width(), textGeom.height()) / 3.f;

        DGL_Color3fv(expiredMarkColor);
        DGL_Begin(DGL_TRIANGLES);
        DGL_Vertex2f(textGeom.width(), 0);
        DGL_Vertex2f(textGeom.width(), markSize);
        DGL_Vertex2f(textGeom.width() - markSize, 0);
        DGL_End();
    }

    // Mark triggered?
    if (button.bindContextAssociation() & InputControl::Triggered)
    {
        const int markSize = .5f + de::min(textGeom.width(), textGeom.height()) / 3.f;

        DGL_Color3fv(triggeredMarkColor);
        DGL_Begin(DGL_TRIANGLES);
        DGL_Vertex2f(0, 0);
        DGL_Vertex2f(markSize, 0);
        DGL_Vertex2f(0, markSize);
        DGL_End();
    }

    endDrawStateForVisual(&origin);

    if (geometry)
    {
        std::memcpy(&geometry->origin, &origin, sizeof(geometry->origin));
        geometry->size.width  = textGeom.width();
        geometry->size.height = textGeom.height();
    }

#undef BORDER
}

void Rend_RenderAxisStateVisual(InputDevice & /*device*/, int /*axisID*/, const Point2Raw *origin,
    RectRaw *geometry)
{
    if (geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    //const inputdevaxis_t &axis = device.axis(axisID);

    initDrawStateForVisual(origin);

    endDrawStateForVisual(origin);
}

void Rend_RenderHatStateVisual(InputDevice & /*device*/, int /*hatID*/, const Point2Raw *origin,
    RectRaw *geometry)
{
    if (geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    //const inputdevhat_t &hat = device.hat(hatID);

    initDrawStateForVisual(origin);

    endDrawStateForVisual(origin);
}

// Input device control types:
enum inputdev_controltype_t
{
    IDC_KEY,
    IDC_AXIS,
    IDC_HAT,
    NUM_INPUT_DEVICE_CONTROL_TYPES
};

struct inputdev_layout_control_t
{
    inputdev_controltype_t type;
    uint id;
};

struct inputdev_layout_controlgroup_t
{
    inputdev_layout_control_t *controls;
    uint numControls;
};

// Defines the order of controls in the visual.
struct inputdev_layout_t
{
    inputdev_layout_controlgroup_t *groups;
    uint numGroups;
};

static void drawControlGroup(InputDevice &device, const inputdev_layout_controlgroup_t *group,
    Point2Raw *_origin, RectRaw *retGeometry)
{
#define SPACING  2

    if (retGeometry)
    {
        retGeometry->origin.x = retGeometry->origin.y = 0;
        retGeometry->size.width = retGeometry->size.height = 0;
    }

    if (!group) return;

    Point2Raw origin;
    origin.x = _origin? _origin->x : 0;
    origin.y = _origin? _origin->y : 0;

    Rect *grpGeom = nullptr;
    RectRaw ctrlGeom{};
    for (uint i = 0; i < group->numControls; ++i)
    {
        const inputdev_layout_control_t *ctrl = group->controls + i;

        switch (ctrl->type)
        {
        case IDC_AXIS: Rend_RenderAxisStateVisual(device, ctrl->id, &origin, &ctrlGeom); break;
        case IDC_KEY:  Rend_RenderButtonStateVisual(device, ctrl->id, &origin, &ctrlGeom); break;
        case IDC_HAT:  Rend_RenderHatStateVisual(device, ctrl->id, &origin, &ctrlGeom); break;

        default:
            App_Error("drawControlGroup: Unknown inputdev_controltype_t: %i.", (int)ctrl->type);
        }

        if (ctrlGeom.size.width > 0 && ctrlGeom.size.height > 0)
        {
            origin.x += ctrlGeom.size.width + SPACING;

            if (grpGeom)
                Rect_UniteRaw(grpGeom, &ctrlGeom);
            else
                grpGeom = Rect_NewFromRaw(&ctrlGeom);
        }
    }

    if (grpGeom)
    {
        if (retGeometry)
        {
            Rect_Raw(grpGeom, retGeometry);
        }

        Rect_Delete(grpGeom);
    }

#undef SPACING
}

/**
 * Render a visual representation of the current state of the specified device.
 */
void Rend_RenderInputDeviceStateVisual(InputDevice &device, const inputdev_layout_t *layout,
    const Point2Raw *origin, Size2Raw *retVisualDimensions)
{
#define SPACING  2

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    if (retVisualDimensions)
    {
        retVisualDimensions->width = retVisualDimensions->height = 0;
    }

    if (novideo || isDedicated) return; // Not for us.
    if (!layout) return;

    // Init render state.
    FR_SetFont(fontFixed);
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetColorAndAlpha(1, 1, 1, 1);
    initDrawStateForVisual(origin);

    Point2Raw offset{};
    Rect *visualGeom = nullptr;

    // Draw device name first.
    if (!device.title().isEmpty())
    {
        Size2Raw size;

        DGL_Enable(DGL_TEXTURE_2D);
        FR_DrawText(device.title(), nullptr/*no offset*/);
        DGL_Disable(DGL_TEXTURE_2D);

        FR_TextSize(&size, device.title());
        visualGeom = Rect_NewWithOriginSize2(offset.x, offset.y, size.width, size.height);

        offset.y += size.height + SPACING;
    }

    // Draw control groups.
    for (uint i = 0; i < layout->numGroups; ++i)
    {
        const inputdev_layout_controlgroup_t *grp = &layout->groups[i];
        RectRaw grpGeometry{};

        drawControlGroup(device, grp, &offset, &grpGeometry);

        if (grpGeometry.size.width > 0 && grpGeometry.size.height > 0)
        {
            if (visualGeom)
                Rect_UniteRaw(visualGeom, &grpGeometry);
            else
                visualGeom = Rect_NewFromRaw(&grpGeometry);

            offset.y = Rect_Y(visualGeom) + Rect_Height(visualGeom) + SPACING;
        }
    }

    // Back to previous render state.
    endDrawStateForVisual(origin);
    FR_PopAttrib();

    // Return the united geometry dimensions?
    if (visualGeom && retVisualDimensions)
    {
        retVisualDimensions->width  = Rect_Width(visualGeom);
        retVisualDimensions->height = Rect_Height(visualGeom);
    }

#undef SPACING
}

void I_DebugDrawer()
{
#define SPACING      2
#define NUMITEMS(x)  (sizeof(x) / sizeof((x)[0]))

    // Keyboard (Standard US English layout):
    static inputdev_layout_control_t keyGroup1[] = {
        { IDC_KEY,  27 }, // escape
        { IDC_KEY, 132 }, // f1
        { IDC_KEY, 133 }, // f2
        { IDC_KEY, 134 }, // f3
        { IDC_KEY, 135 }, // f4
        { IDC_KEY, 136 }, // f5
        { IDC_KEY, 137 }, // f6
        { IDC_KEY, 138 }, // f7
        { IDC_KEY, 139 }, // f8
        { IDC_KEY, 140 }, // f9
        { IDC_KEY, 141 }, // f10
        { IDC_KEY, 142 }, // f11
        { IDC_KEY, 143 }  // f12
    };
    static inputdev_layout_control_t keyGroup2[] = {
        { IDC_KEY,  96 }, // tilde
        { IDC_KEY,  49 }, // 1
        { IDC_KEY,  50 }, // 2
        { IDC_KEY,  51 }, // 3
        { IDC_KEY,  52 }, // 4
        { IDC_KEY,  53 }, // 5
        { IDC_KEY,  54 }, // 6
        { IDC_KEY,  55 }, // 7
        { IDC_KEY,  56 }, // 8
        { IDC_KEY,  57 }, // 9
        { IDC_KEY,  48 }, // 0
        { IDC_KEY,  45 }, // -
        { IDC_KEY,  61 }, // =
        { IDC_KEY, 127 }  // backspace
    };
    static inputdev_layout_control_t keyGroup3[] = {
        { IDC_KEY,   9 }, // tab
        { IDC_KEY, 113 }, // q
        { IDC_KEY, 119 }, // w
        { IDC_KEY, 101 }, // e
        { IDC_KEY, 114 }, // r
        { IDC_KEY, 116 }, // t
        { IDC_KEY, 121 }, // y
        { IDC_KEY, 117 }, // u
        { IDC_KEY, 105 }, // i
        { IDC_KEY, 111 }, // o
        { IDC_KEY, 112 }, // p
        { IDC_KEY,  91 }, // {
        { IDC_KEY,  93 }, // }
        { IDC_KEY,  92 }, // bslash
    };
    static inputdev_layout_control_t keyGroup4[] = {
        { IDC_KEY, 145 }, // capslock
        { IDC_KEY,  97 }, // a
        { IDC_KEY, 115 }, // s
        { IDC_KEY, 100 }, // d
        { IDC_KEY, 102 }, // f
        { IDC_KEY, 103 }, // g
        { IDC_KEY, 104 }, // h
        { IDC_KEY, 106 }, // j
        { IDC_KEY, 107 }, // k
        { IDC_KEY, 108 }, // l
        { IDC_KEY,  59 }, // semicolon
        { IDC_KEY,  39 }, // apostrophe
        { IDC_KEY,  13 }  // return
    };
    static inputdev_layout_control_t keyGroup5[] = {
        { IDC_KEY, 159 }, // shift
        { IDC_KEY, 122 }, // z
        { IDC_KEY, 120 }, // x
        { IDC_KEY,  99 }, // c
        { IDC_KEY, 118 }, // v
        { IDC_KEY,  98 }, // b
        { IDC_KEY, 110 }, // n
        { IDC_KEY, 109 }, // m
        { IDC_KEY,  44 }, // comma
        { IDC_KEY,  46 }, // period
        { IDC_KEY,  47 }, // fslash
        { IDC_KEY, 159 }, // shift
    };
    static inputdev_layout_control_t keyGroup6[] = {
        { IDC_KEY, 160 }, // ctrl
        { IDC_KEY,   0 }, // ???
        { IDC_KEY, 161 }, // alt
        { IDC_KEY,  32 }, // space
        { IDC_KEY, 161 }, // alt
        { IDC_KEY,   0 }, // ???
        { IDC_KEY,   0 }, // ???
        { IDC_KEY, 160 }  // ctrl
    };
    static inputdev_layout_control_t keyGroup7[] = {
        { IDC_KEY, 170 }, // printscrn
        { IDC_KEY, 146 }, // scrolllock
        { IDC_KEY, 158 }  // pause
    };
    static inputdev_layout_control_t keyGroup8[] = {
        { IDC_KEY, 162 }, // insert
        { IDC_KEY, 166 }, // home
        { IDC_KEY, 164 }  // pageup
    };
    static inputdev_layout_control_t keyGroup9[] = {
        { IDC_KEY, 163 }, // delete
        { IDC_KEY, 167 }, // end
        { IDC_KEY, 165 }, // pagedown
    };
    static inputdev_layout_control_t keyGroup10[] = {
        { IDC_KEY, 130 }, // up
        { IDC_KEY, 129 }, // left
        { IDC_KEY, 131 }, // down
        { IDC_KEY, 128 }, // right
    };
    static inputdev_layout_control_t keyGroup11[] = {
        { IDC_KEY, 144 }, // numlock
        { IDC_KEY, 172 }, // divide
        { IDC_KEY, DDKEY_MULTIPLY }, // multiply
        { IDC_KEY, 168 }  // subtract
    };
    static inputdev_layout_control_t keyGroup12[] = {
        { IDC_KEY, 147 }, // pad 7
        { IDC_KEY, 148 }, // pad 8
        { IDC_KEY, 149 }, // pad 9
        { IDC_KEY, 169 }, // add
    };
    static inputdev_layout_control_t keyGroup13[] = {
        { IDC_KEY, 150 }, // pad 4
        { IDC_KEY, 151 }, // pad 5
        { IDC_KEY, 152 }  // pad 6
    };
    static inputdev_layout_control_t keyGroup14[] = {
        { IDC_KEY, 153 }, // pad 1
        { IDC_KEY, 154 }, // pad 2
        { IDC_KEY, 155 }, // pad 3
        { IDC_KEY, 171 }  // enter
    };
    static inputdev_layout_control_t keyGroup15[] = {
        { IDC_KEY, 156 }, // pad 0
        { IDC_KEY, 157 }  // pad .
    };
    static inputdev_layout_controlgroup_t keyGroups[] = {
        { keyGroup1, NUMITEMS(keyGroup1) },
        { keyGroup2, NUMITEMS(keyGroup2) },
        { keyGroup3, NUMITEMS(keyGroup3) },
        { keyGroup4, NUMITEMS(keyGroup4) },
        { keyGroup5, NUMITEMS(keyGroup5) },
        { keyGroup6, NUMITEMS(keyGroup6) },
        { keyGroup7, NUMITEMS(keyGroup7) },
        { keyGroup8, NUMITEMS(keyGroup8) },
        { keyGroup9, NUMITEMS(keyGroup9) },
        { keyGroup10, NUMITEMS(keyGroup10) },
        { keyGroup11, NUMITEMS(keyGroup11) },
        { keyGroup12, NUMITEMS(keyGroup12) },
        { keyGroup13, NUMITEMS(keyGroup13) },
        { keyGroup14, NUMITEMS(keyGroup14) },
        { keyGroup15, NUMITEMS(keyGroup15) }
    };
    static inputdev_layout_t keyLayout = { keyGroups, NUMITEMS(keyGroups) };

    // Mouse:
    static inputdev_layout_control_t mouseGroup1[] = {
        { IDC_AXIS, 0 }, // x-axis
        { IDC_AXIS, 1 }, // y-axis
    };
    static inputdev_layout_control_t mouseGroup2[] = {
        { IDC_KEY, 0 }, // left
        { IDC_KEY, 1 }, // middle
        { IDC_KEY, 2 }  // right
    };
    static inputdev_layout_control_t mouseGroup3[] = {
        { IDC_KEY, 3 }, // wheelup
        { IDC_KEY, 4 }  // wheeldown
    };
    static inputdev_layout_control_t mouseGroup4[] = {
        { IDC_KEY,  5 },
        { IDC_KEY,  6 },
        { IDC_KEY,  7 },
        { IDC_KEY,  8 },
        { IDC_KEY,  9 },
        { IDC_KEY, 10 },
        { IDC_KEY, 11 },
        { IDC_KEY, 12 },
        { IDC_KEY, 13 },
        { IDC_KEY, 14 },
        { IDC_KEY, 15 }
    };
    static inputdev_layout_controlgroup_t mouseGroups[] = {
        { mouseGroup1, NUMITEMS(mouseGroup1) },
        { mouseGroup2, NUMITEMS(mouseGroup2) },
        { mouseGroup3, NUMITEMS(mouseGroup3) },
        { mouseGroup4, NUMITEMS(mouseGroup4) }
    };
    static inputdev_layout_t mouseLayout = { mouseGroups, NUMITEMS(mouseGroups) };

    // Joystick:
    static inputdev_layout_control_t joyGroup1[] = {
        { IDC_AXIS, 0 }, // x-axis
        { IDC_AXIS, 1 }, // y-axis
        { IDC_AXIS, 2 }, // z-axis
        { IDC_AXIS, 3 }  // w-axis
    };
    static inputdev_layout_control_t joyGroup2[] = {
        { IDC_AXIS,  4 },
        { IDC_AXIS,  5 },
        { IDC_AXIS,  6 },
        { IDC_AXIS,  7 },
        { IDC_AXIS,  8 },
        { IDC_AXIS,  9 },
        { IDC_AXIS, 10 },
        { IDC_AXIS, 11 },
        { IDC_AXIS, 12 },
        { IDC_AXIS, 13 },
        { IDC_AXIS, 14 },
        { IDC_AXIS, 15 },
        { IDC_AXIS, 16 },
        { IDC_AXIS, 17 }
    };
    static inputdev_layout_control_t joyGroup3[] = {
        { IDC_AXIS, 18 },
        { IDC_AXIS, 19 },
        { IDC_AXIS, 20 },
        { IDC_AXIS, 21 },
        { IDC_AXIS, 22 },
        { IDC_AXIS, 23 },
        { IDC_AXIS, 24 },
        { IDC_AXIS, 25 },
        { IDC_AXIS, 26 },
        { IDC_AXIS, 27 },
        { IDC_AXIS, 28 },
        { IDC_AXIS, 29 },
        { IDC_AXIS, 30 },
        { IDC_AXIS, 31 }
    };
    static inputdev_layout_control_t joyGroup4[] = {
        { IDC_HAT,  0 },
        { IDC_HAT,  1 },
        { IDC_HAT,  2 },
        { IDC_HAT,  3 }
    };
    static inputdev_layout_control_t joyGroup5[] = {
        { IDC_KEY,  0 },
        { IDC_KEY,  1 },
        { IDC_KEY,  2 },
        { IDC_KEY,  3 },
        { IDC_KEY,  4 },
        { IDC_KEY,  5 },
        { IDC_KEY,  6 },
        { IDC_KEY,  7 }
    };
    static inputdev_layout_control_t joyGroup6[] = {
        { IDC_KEY,  8 },
        { IDC_KEY,  9 },
        { IDC_KEY, 10 },
        { IDC_KEY, 11 },
        { IDC_KEY, 12 },
        { IDC_KEY, 13 },
        { IDC_KEY, 14 },
        { IDC_KEY, 15 },
    };
    static inputdev_layout_control_t joyGroup7[] = {
        { IDC_KEY, 16 },
        { IDC_KEY, 17 },
        { IDC_KEY, 18 },
        { IDC_KEY, 19 },
        { IDC_KEY, 20 },
        { IDC_KEY, 21 },
        { IDC_KEY, 22 },
        { IDC_KEY, 23 },
    };
    static inputdev_layout_control_t joyGroup8[] = {
        { IDC_KEY, 24 },
        { IDC_KEY, 25 },
        { IDC_KEY, 26 },
        { IDC_KEY, 27 },
        { IDC_KEY, 28 },
        { IDC_KEY, 29 },
        { IDC_KEY, 30 },
        { IDC_KEY, 31 },
    };
    static inputdev_layout_controlgroup_t joyGroups[] = {
        { joyGroup1, NUMITEMS(joyGroup1) },
        { joyGroup2, NUMITEMS(joyGroup2) },
        { joyGroup3, NUMITEMS(joyGroup3) },
        { joyGroup4, NUMITEMS(joyGroup4) },
        { joyGroup5, NUMITEMS(joyGroup5) },
        { joyGroup6, NUMITEMS(joyGroup6) },
        { joyGroup7, NUMITEMS(joyGroup7) },
        { joyGroup8, NUMITEMS(joyGroup8) }
    };
    static inputdev_layout_t joyLayout = { joyGroups, NUMITEMS(joyGroups) };

    Point2Raw origin = {{{2, 2}}};
    Size2Raw dimensions{};

    if (novideo || isDedicated) return; // Not for us.

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    // Disabled?
    if (!devRendKeyState && !devRendMouseState && !devRendJoyState) return;

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);

    if (devRendKeyState)
    {
        Rend_RenderInputDeviceStateVisual(InputSystem::get().device(IDEV_KEYBOARD), &keyLayout, &origin, &dimensions);
        origin.y += dimensions.height + SPACING;
    }

    if (devRendMouseState)
    {
        Rend_RenderInputDeviceStateVisual(InputSystem::get().device(IDEV_MOUSE), &mouseLayout, &origin, &dimensions);
        origin.y += dimensions.height + SPACING;
    }

    if (devRendJoyState)
    {
        Rend_RenderInputDeviceStateVisual(InputSystem::get().device(IDEV_JOY1), &joyLayout, &origin, &dimensions);
    }

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

#undef NUMITEMS
#undef SPACING
}

void I_DebugDrawerConsoleRegister()
{
    C_VAR_BYTE("rend-dev-input-joy-state",   &devRendJoyState,   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-input-key-state",   &devRendKeyState,   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-input-mouse-state", &devRendMouseState, CVF_NO_ARCHIVE, 0, 1);
}

#endif // DE_DEBUG
