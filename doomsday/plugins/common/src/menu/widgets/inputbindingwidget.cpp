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

            if(!strncmp(ptr, "key", 3) || strstr(ptr, "-button") ||
               !strncmp(ptr, "mouse-left", 10) || !strncmp(ptr, "mouse-middle", 12) ||
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

void InputBindingWidget::draw(Point2Raw const *origin)
{
    bindingdrawerdata_t draw;
    char buf[1024];

    if(binds->controlName)
    {
        B_BindingsForControl(0, binds->controlName, BFCI_BOTH, buf, sizeof(buf));
    }
    else
    {
        B_BindingsForCommand(binds->command, buf, sizeof(buf));
    }
    draw.origin.x = origin->x;
    draw.origin.y = origin->y;
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
        if(hasAction(MNA_ACTIVE))
        {
            execAction(MNA_ACTIVE);
            return true;
        }
        break;

    default: break;
    }

    return false; // Not eaten.
}

void InputBindingWidget::updateGeometry(Page * /*page*/)
{
    // @todo calculate visible dimensions properly!
    Rect_SetWidthHeight(geometry(), 60, 10 * SMALL_SCALE);
}

int InputBindingWidget::handleEvent_Privileged(event_t *ev)
{
    DENG2_ASSERT(ev != 0);

    // We're interested in key or button down events.
    if(isActive() && ev->type == EV_SYMBOLIC)
    {
        char const *bindContext = "game";
        char const *symbol = 0;
        char cmd[512];

#ifndef __64BIT__
        symbol = (char const *) ev->data1;
#else
        {
            uint64_t address = (uint32_t) ev->data2;
            address <<= 32;
            address |= (uint32_t) ev->data1;
            symbol = (char const *) address;
        }
#endif

        if(strncmp(symbol, "echo-", 5))
        {
            return false;
        }
        if(!strncmp(symbol, "echo-key-", 9) && strcmp(symbol + strlen(symbol) - 5, "-down"))
        {
           return false;
        }

        if(binds->bindContext)
        {
            bindContext = binds->bindContext;

            if((!strcmp(bindContext, "menu") || !strcmp(bindContext, "shortcut")) &&
               !strcmp(symbol + 5, "key-delete-down"))
            {
                throw Error("InputBindingWidget::handleEvent_Priviledged", "The Delete key in the Menu context is reserved for deleting bindings");
                return false;
            }
        }

        if(binds->command)
        {
            char const *extraCondition = (binds->flags & CCF_MULTIPLAYER? " + multiplayer" : "");
            sprintf(cmd, "bindevent {%s:%s%s} {%s}", bindContext, &symbol[5], extraCondition, binds->command);

            // Check for repeats.
            if(binds->flags & CCF_REPEAT)
            {
                char const *downPtr = 0;
                char temp[256];

                downPtr = strstr(symbol + 5, "-down");
                if(downPtr)
                {
                    char temp2[256];

                    std::memset(temp2, 0, sizeof(temp2));
                    strncpy(temp2, symbol + 5, downPtr - symbol - 5);
                    sprintf(temp, "; bindevent {%s:%s-repeat} {%s}", bindContext, temp2, binds->command);
                    strcat(cmd, temp);
                }
            }
        }
        else if(binds->controlName)
        {
            // Have to exclude the state part.
            dd_bool inv      = (binds->flags & CCF_INVERSE) != 0;
            dd_bool isStaged = (binds->flags & CCF_STAGED) != 0;
            char const *end = strchr(symbol + 5, '-');
            char temp3[256];
            char extra[256];

            end = strchr(end + 1, '-');

            if(!end)
            {
                Con_Error("what! %s\n", symbol);
            }

            std::memset(temp3, 0, sizeof(temp3));
            strncpy(temp3, symbol + 5, end - symbol - 5);

            // Check for inverse.
            if(!strncmp(end, "-neg", 4))
            {
                inv = !inv;
            }
            if(isStaged && (!strncmp(temp3, "key-", 4) || strstr(temp3, "-button") ||
                            !strcmp(temp3, "mouse-left") || !strcmp(temp3, "mouse-middle") ||
                            !strcmp(temp3, "mouse-right")))
            {
                // Staging is for keys and buttons.
                strcat(temp3, "-staged");
            }
            if(inv)
            {
                strcat(temp3, "-inverse");
            }

            strcpy(extra, "");
            if(binds->flags & CCF_SIDESTEP_MODIFIER)
            {
                sprintf(cmd, "bindcontrol sidestep {%s + modifier-1-down}", temp3);
                DD_Execute(true, cmd);

                strcpy(extra, " + modifier-1-up");
            }

            sprintf(cmd, "bindcontrol {%s} {%s%s}", binds->controlName, temp3, extra);
        }

        LOGDEV_INPUT_MSG("MNBindings_PrivilegedResponder: %s") << cmd;
        DD_Execute(true, cmd);

        // We've finished the grab.
        setFlags(Active, UnsetFlags);
        DD_SetInteger(DD_SYMBOLIC_ECHO, false);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        return true;
    }

    return false;
}

char const *InputBindingWidget::controlName()
{
    DENG2_ASSERT(binds != 0);
    // Map to a text definition?
    if(PTR2INT(binds->text) > 0 && PTR2INT(binds->text) < NUMTEXT)
    {
        return GET_TXT(PTR2INT(binds->text));
    }
    return binds->text;
}

} // namespace menu
} // namespace common
