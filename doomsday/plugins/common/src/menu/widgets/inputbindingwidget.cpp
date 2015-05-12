/** @file inputbindingwidget.cpp  UI widget for manipulating input bindings.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "menu/widgets/inputbindingwidget.h"

#include "hu_menu.h" // menu sounds
#include "m_ctrl.h" // controlconfig_t, etc..
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

#define SMALL_SCALE .75f

// Binding iteration flags
#define MIBF_IGNORE_REPEATS     0x1

enum bindingitertype_t
{
    MIBT_KEY,
    MIBT_MOUSE,
    MIBT_JOY
};

static void deleteBinding(bindingitertype_t /*type*/, int bid, char const * /*name*/, dd_bool /*isInverse*/, void * /*data*/)
{
    DD_Executef(true, "delbind %i", bid);
}

static char const *findInString(char const *str, char const *token, int n)
{
    int tokenLen = strlen(token);
    char const *at = strstr(str, token);

    // Not there at all?
    if(!at) return 0;

    if(at - str <= n - tokenLen)
    {
        return at;
    }

    // Past the end.
    return 0;
}

static void iterateBindings(controlconfig_t const *binds, char const *bindings, int flags, void *data,
    void (*callback)(bindingitertype_t type, int bid, char const *ev, dd_bool isInverse, void *data))
{
    DENG2_ASSERT(binds != 0);

    char const *ptr = strchr(bindings, ':');
    char const *begin, *end, *end2, *k, *bindingStart, *bindingEnd;
    char buf[80], *b;
    dd_bool isInverse;
    int bid;

    std::memset(buf, 0, sizeof(buf));

    while(ptr)
    {
        // Read the binding identifier.
        for(k = ptr; k > bindings && *k != '@'; --k);

        if(*k == '@')
        {
            for(begin = k - 1; begin > bindings && isdigit(*(begin - 1)); --begin) {}
            bid = strtol(begin, NULL, 10);
        }
        else
        {
            // No identifier??
            bid = 0;
        }

        // Find the end of the entire binding.
        bindingStart = k + 1;
        bindingEnd = strchr(bindingStart, '@');
        if(!bindingEnd)
        {
            // Then point to the end of the string.
            bindingEnd = strchr(k + 1, 0);
        }

        ptr++;
        end = strchr(ptr, '-');
        if(!end)
            return;

        end++;
        b = buf;
        while(*end && *end != ' ' && *end != '-' && *end != '+')
        {
            *b++ = *end++;
        }
        *b = 0;

        end2 = strchr(end, ' ');
        if(!end2)
            end = end + strlen(end); // Then point to the end.
        else
            end = end2;

        if(!findInString(bindingStart, "modifier-1-down", bindingEnd - bindingStart) &&
           (!(flags & MIBF_IGNORE_REPEATS) || !findInString(ptr, "-repeat", end - ptr)))
        {
            isInverse = (findInString(ptr, "-inverse", end - ptr) != NULL);

            if(!strncmp(ptr, "key", 3)           ||
               !strncmp(ptr, "joy-button", 10)   ||
               !strncmp(ptr, "mouse-left", 10)   ||
               !strncmp(ptr, "mouse-middle", 12) ||
               !strncmp(ptr, "mouse-right", 11))
            {
                if(((binds->flags & CCF_INVERSE) && isInverse) ||
                   ((binds->flags & CCF_NON_INVERSE) && !isInverse) ||
                   !(binds->flags & (CCF_INVERSE | CCF_NON_INVERSE)))
                {
                    callback(!strncmp(ptr, "key", 3)? MIBT_KEY :
                             !strncmp(ptr, "mouse", 5)? MIBT_MOUSE : MIBT_JOY, bid, buf,
                             isInverse, data);
                }
            }
            else
            {
                if(!(binds->flags & (CCF_INVERSE | CCF_NON_INVERSE)) || (binds->flags & CCF_INVERSE))
                {
                    isInverse = !isInverse;
                }
                if(!strncmp(ptr, "joy", 3))
                {
                    callback(MIBT_JOY, bid, buf, isInverse, data);
                }
                else if(!strncmp(ptr, "mouse", 5))
                {
                    callback(MIBT_MOUSE, bid, buf, isInverse, data);
                }
            }
        }

        ptr = end;
        while(*ptr == ' ') { ptr++; }

        ptr = strchr(ptr, ':');
    }
}

InputBindingWidget::InputBindingWidget()
    : Widget()
    , binds(0)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
}

InputBindingWidget::~InputBindingWidget()
{}

static void drawSmallText(char const *string, int x, int y, float alpha)
{
    int height = FR_TextHeight(string);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(x, y + height/2, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-x, -y - height/2, 0);

    FR_SetColorAndAlpha(1, 1, 1, alpha);
    FR_DrawTextXY3(string, x, y, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

struct bindingdrawerdata_t
{
    Point2Raw origin;
    float alpha;
};

static void drawBinding(bindingitertype_t type, int /*bid*/, const char *name,
    dd_bool isInverse, void *context)
{
#define BIND_GAP                (2)

#if __JHERETIC__
    static float const bgRGB[] = { 0, .5f, 0 };
#elif __JHEXEN__
    static float const bgRGB[] = { .5f, 0, 0 };
#else
    static float const bgRGB[] = { 0, 0, 0 };
#endif

    bindingdrawerdata_t *d = (bindingdrawerdata_t *)context;

    FR_SetFont(FID(GF_FONTA));

    if(type == MIBT_KEY)
    {
        int const width  = FR_TextWidth(name);
        int const height = FR_TextHeight(name);

        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(d->origin.x, d->origin.y, width * SMALL_SCALE + 2, height, bgRGB[0], bgRGB[1], bgRGB[2], d->alpha * .6f);

        DGL_Enable(DGL_TEXTURE_2D);
        drawSmallText(name, d->origin.x + 1, d->origin.y, d->alpha);
        DGL_Disable(DGL_TEXTURE_2D);

        d->origin.x += width * SMALL_SCALE + 2 + BIND_GAP;
    }
    else
    {
        char buf[256];
        sprintf(buf, "%s%c%s", type == MIBT_MOUSE? "mouse" : "joy", isInverse? '-' : '+', name);

        int const width  = FR_TextWidth(buf);
        ///int const height = FR_TextHeight(temp);

        DGL_Enable(DGL_TEXTURE_2D);
        drawSmallText(buf, d->origin.x, d->origin.y, d->alpha);
        DGL_Disable(DGL_TEXTURE_2D);

        d->origin.x += width * SMALL_SCALE + BIND_GAP;
    }

#undef BIND_GAP
}

void InputBindingWidget::draw() const
{
    char buf[1024];
    if(binds->controlName)
    {
        B_BindingsForControl(0, binds->controlName, BFCI_BOTH, buf, sizeof(buf));
    }
    else
    {
        B_BindingsForCommand(binds->command, buf, sizeof(buf));
    }

    bindingdrawerdata_t draw;
    draw.origin.x = geometry().topLeft.x;
    draw.origin.y = geometry().topLeft.y;
    draw.alpha = mnRendState->pageAlpha;
    iterateBindings(binds, buf, MIBF_IGNORE_REPEATS, &draw, drawBinding);
}

int InputBindingWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_DELETE: {
        char buf[1024];

        S_LocalSound(SFX_MENU_CANCEL, NULL);
        if(binds->controlName)
        {
            B_BindingsForControl(0, binds->controlName, BFCI_BOTH, buf, sizeof(buf));
        }
        else
        {
            B_BindingsForCommand(binds->command, buf, sizeof(buf));
        }

        iterateBindings(binds, buf, 0, NULL, deleteBinding);

        // If deleting the menuselect binding, automatically rebind it Return;
        // otherwise the user would be stuck without a way to make further bindings.
        if(binds->command && !strcmp(binds->command, "menuselect"))
        {
            DD_Execute(true, "bindevent menu:key-return menuselect");
        }
        return true; }

    case MCMD_SELECT:
        S_LocalSound(SFX_MENU_CYCLE, NULL);
        setFlags(Active);
        if(hasAction(Activated))
        {
            execAction(Activated);
            return true;
        }
        break;

    default: break;
    }

    return false; // Not eaten.
}

void InputBindingWidget::updateGeometry()
{
    // @todo calculate visible dimensions properly!
    geometry().setSize(Vector2ui(60, 10 * SMALL_SCALE));
}

/**
 * Read the symbolic descriptor from the given @a event.
 */
static String symbolicDescriptor(event_t const &event)
{
    if(event.type == EV_SYMBOLIC)
    {
#ifndef __64BIT__
        String symbol = (char const *) event.data1;
#else
        String symbol = (char const *)( (duint64(event.data2) << 32) | duint64(event.data1) );
#endif
        if(symbol.beginsWith("echo-"))
        {
            return symbol.substr(5);
        }
    }
    return ""; // No symbolic descriptor.
}

int InputBindingWidget::handleEvent_Privileged(event_t const &event)
{
    LOG_AS("InputBindingWidget");

    // Only handle events when active.
    if(!isActive()) return false;

    // We're only interested in events with an echoed, symbolic descriptor.
    String symbol = symbolicDescriptor(event);
    if(symbol.isEmpty()) return false;

    // We're only interested in button down events.
    if((symbol.beginsWith("key-")         ||
        symbol.beginsWith("joy-button")   ||
        symbol.beginsWith("mouse-left")   ||
        symbol.beginsWith("mouse-middle") ||
        symbol.beginsWith("mouse-right")) && !symbol.endsWith("-down"))
    {
        return false;
    }

    String const context = bindContext();

    // The Delete key in the Menu context is reserved for deleting bindings
    if((!context.compareWithCase("menu") || !context.compareWithCase("shortcut")) &&
       symbol.beginsWith("key-delete-down"))
    {
        return false;
    }

    String cmd;
    if(binds->command)
    {
        cmd = String("bindevent {%1:%2%3} {%4}")
                  .arg(context)
                  .arg(symbol)
                  .arg(binds->flags & CCF_MULTIPLAYER? " + multiplayer" : "")
                  .arg(binds->command);

        // Check for repeats.
        if((binds->flags & CCF_REPEAT) && symbol.endsWith("-down"))
        {
            cmd += String("; bindevent {%1:%2-repeat} {%3}")
                       .arg(context)
                       .arg(symbol.left(symbol.length() - 5))
                       .arg(binds->command);
        }
    }
    else if(binds->controlName)
    {
        String stateFlags;

        // Extract the symbolic key/button name (exclude the state part).
        int const endOfName = symbol.indexOf('-', symbol.indexOf('-') + 1);
        if(endOfName < 0)
        {
            throw Error("InputBindingWidget::handleEvent_Privileged", "Invalid symbol:" + symbol);
        }
        String const name = symbol.left(endOfName);

        // Staged?
        if(binds->flags & CCF_STAGED)
        {
            // Staging is buttons.
            if(name.beginsWith("key-")         ||
               name.beginsWith("joy-button")   ||
               name.beginsWith("mouse-left")   ||
               name.beginsWith("mouse-middle") ||
               name.beginsWith("mouse-right"))
            {
                stateFlags += "-staged";
            }
        }

        // Inverted?
        bool inv = (binds->flags & CCF_INVERSE) != 0;
        if(symbol.substr(endOfName).beginsWith("-neg"))
        {
            inv = !inv;
        }
        if(inv)
        {
            stateFlags += "-inverse";
        }

        cmd = String("bindcontrol {%1} {%2%3%4}")
                  .arg(binds->controlName)
                  .arg(name)
                  .arg(stateFlags)
                  .arg((binds->flags & CCF_SIDESTEP_MODIFIER)? " + modifier-1-up" : "");

        if(binds->flags & CCF_SIDESTEP_MODIFIER)
        {
            cmd += String("; bindcontrol sidestep {%1%2 + modifier-1-down}")
                       .arg(name)
                       .arg(stateFlags);
        }
    }

    LOGDEV_INPUT_MSG("PrivilegedResponder: ") << cmd;
    DD_Execute(true, cmd.toUtf8().constData());

    // We've finished the grab.
    setFlags(Active, UnsetFlags);
    DD_SetInteger(DD_SYMBOLIC_ECHO, false);
    S_LocalSound(SFX_MENU_ACCEPT, nullptr);
    return true;
}

char const *InputBindingWidget::controlName() const
{
    DENG2_ASSERT(binds);
    // Map to a text definition?
    if(PTR2INT(binds->text) > 0 && PTR2INT(binds->text) < NUMTEXT)
    {
        return GET_TXT(PTR2INT(binds->text));
    }
    return binds->text;
}

String InputBindingWidget::bindContext() const
{
    DENG2_ASSERT(binds);
    return (binds->bindContext? binds->bindContext : "game");
}

} // namespace menu
} // namespace common
