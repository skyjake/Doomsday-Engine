/** @file inputbindingwidget.cpp  UI widget for manipulating input bindings.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "hu_menu.h"   // menu sounds
#include "hu_stuff.h"
#include "m_ctrl.h"    // controlconfig_t, etc..
#include "menu/page.h" // mnRendState

#include <map>
#include <tuple>

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

static const char *findInString(const char *str, const char *token, int n)
{
    int tokenLen = strlen(token);
    const char *at = strstr(str, token);

    // Not there at all?
    if(!at) return 0;

    if(at - str <= n - tokenLen)
    {
        return at;
    }

    // Past the end.
    return 0;
}

static void drawSmallText(const char *string, int x, int y, float alpha)
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

DE_PIMPL(InputBindingWidget)
{
    bool needGeometry = true; ///< Recalculate geometry based on bindings.
    const int maxWidth = SCREENWIDTH * 55 / 100;

    Impl(Public *i) : Base(i)
    {}

    void iterateBindings(
        int flags,
        const std::function<
            void(bindingitertype_t type, int bid, const char *ev, dd_bool isInverse)>
            &callback) const
    {
        // Bindings are collected to this map so they're iterated in order.
        std::map<bindingitertype_t, std::list<std::tuple<int, std::string, bool>>> bindings;

        const controlconfig_t *binds = self().binds;
        DE_ASSERT(binds != nullptr);

        char bindingsBuf[1024];
        if (binds->controlName)
        {
            B_BindingsForControl(
                0, binds->controlName, BFCI_BOTH, bindingsBuf, sizeof(bindingsBuf));
        }
        else
        {
            B_BindingsForCommand(binds->command, bindingsBuf, sizeof(bindingsBuf));
        }

        const char *ptr = strchr(bindingsBuf, ':');
        const char *begin, *end, *end2, *k, *bindingStart, *bindingEnd;
        char buf[80], *b;
        dd_bool isInverse;
        int bid;

        std::memset(buf, 0, sizeof(buf));

        while (ptr)
        {
            // Read the binding identifier.
            for (k = ptr; k > bindingsBuf && *k != '@'; --k) {;}

            if (*k == '@')
            {
                for (begin = k - 1; begin > bindingsBuf && isdigit(*(begin - 1)); --begin) {;}
                bid = strtol(begin, NULL, 10);
            }
            else
            {
                // No identifier??
                bid = 0;
            }

            // Find the end of the entire binding.
            bindingStart = k + 1;
            bindingEnd   = strchr(bindingStart, '@');
            if (!bindingEnd)
            {
                // Then point to the end of the string.
                bindingEnd = strchr(k + 1, 0);
            }

            ptr++;
            end = strchr(ptr, '-');
            if (!end) return;

            end++;
            b = buf;
            while (*end && *end != ' ' && *end != '-' && *end != '+')
            {
                *b++ = *end++;
            }
            *b = 0;

            end2 = strchr(end, ' ');
            if (!end2)
            {
                end = end + strlen(end); // Then point to the end.
            }
            else
            {
                end = end2;
            }

            if (!findInString(bindingStart, "modifier-1-down", bindingEnd - bindingStart) &&
                (!(flags & MIBF_IGNORE_REPEATS) || !findInString(ptr, "-repeat", end - ptr)))
            {
                isInverse = (findInString(ptr, "-inverse", end - ptr) != NULL);

                if (!strncmp(ptr, "key", 3) || !strncmp(ptr, "joy-button", 10) ||
                    !strncmp(ptr, "mouse-left", 10) || !strncmp(ptr, "mouse-middle", 12) ||
                    !strncmp(ptr, "mouse-right", 11))
                {
                    if (((binds->flags & CCF_INVERSE) && isInverse) ||
                        ((binds->flags & CCF_NON_INVERSE) && !isInverse) ||
                        !(binds->flags & (CCF_INVERSE | CCF_NON_INVERSE)))
                    {
                        bindings[!strncmp(ptr, "key", 3)
                                     ? MIBT_KEY
                                     : !strncmp(ptr, "mouse", 5) ? MIBT_MOUSE : MIBT_JOY]
                            .emplace_back(bid, buf, isInverse);
                    }
                }
                else
                {
                    if (!(binds->flags & (CCF_INVERSE | CCF_NON_INVERSE)) ||
                        (binds->flags & CCF_INVERSE))
                    {
                        isInverse = !isInverse;
                    }
                    if (!strncmp(ptr, "joy", 3))
                    {
                        bindings[MIBT_JOY].emplace_back(bid, buf, isInverse);
                    }
                    else if (!strncmp(ptr, "mouse", 5))
                    {
                        bindings[MIBT_MOUSE].emplace_back(bid, buf, isInverse);
                    }
                }
            }

            ptr = end;
            while (*ptr == ' ')
            {
                ptr++;
            }
            ptr = strchr(ptr, ':');
        }

        for (auto iterType = bindings.begin(); iterType != bindings.end(); ++iterType)
        {
            for (const auto &bind : iterType->second)
            {
                callback(iterType->first,
                         std::get<0>(bind),
                         std::get<1>(bind).c_str(),
                         std::get<2>(bind));
            }
        }
    }

    Vec2ui measureAndDraw(bool drawing = true) const
    {
        const auto widgetTopLeft = self().geometry().topLeft;

        struct {
            bool  drawing;
            float alpha;
            Vec2i widgetTopLeft;
            Vec2i origin;
            Vec2i size;
        } ctx = {drawing,
                 mnRendState->pageAlpha * self().scrollingFadeout(),
                 widgetTopLeft,
                 {0, 0},
                 {0, 0}};

        if (drawing)
        {
            if (ctx.alpha < .001f) return {};
        }

        iterateBindings(
            MIBF_IGNORE_REPEATS,
            [this, &ctx](bindingitertype_t type, int /*bid*/, const char *name, dd_bool isInverse) {
                static const int BIND_GAP = 2;
#if __JHERETIC__
                static float const bgRGB[] = {0, .5f, 0};
#elif __JHEXEN__
                static float const bgRGB[] = {.5f, 0, 0};
#else
                static float const bgRGB[] = {0, 0, 0};
#endif
                FR_SetFont(FID(GF_FONTA));
                const int lineHeight = FR_TextHeight("W");

                if (type == MIBT_KEY)
                {
                    const int width = FR_TextWidth(name) * SMALL_SCALE;

                    if (ctx.origin.x + width > maxWidth)
                    {
                        ctx.origin.x = 0;
                        ctx.origin.y += lineHeight + 1;
                    }

                    if (ctx.drawing)
                    {
                        DGL_SetNoMaterial();
                        DGL_DrawRectf2Color(ctx.widgetTopLeft.x + ctx.origin.x,
                                            ctx.widgetTopLeft.y + ctx.origin.y,
                                            width + 2,
                                            lineHeight,
                                            bgRGB[0],
                                            bgRGB[1],
                                            bgRGB[2],
                                            ctx.alpha * .6f);

                        DGL_Enable(DGL_TEXTURE_2D);
                        drawSmallText(name,
                                      ctx.widgetTopLeft.x + ctx.origin.x + 1,
                                      ctx.widgetTopLeft.y + ctx.origin.y,
                                      ctx.alpha);
                        DGL_Disable(DGL_TEXTURE_2D);
                    }

                    ctx.origin.x += width + 2 + BIND_GAP;
                }
                else
                {
                    char buf[256];
                    sprintf(buf,
                            "%s%c%s",
                            type == MIBT_MOUSE ? "mouse" : "joy",
                            isInverse ? '-' : '+',
                            name);

                    const int width = FR_TextWidth(buf) * SMALL_SCALE;

                    if (ctx.origin.x + width > maxWidth)
                    {
                        ctx.origin.x = 0;
                        ctx.origin.y += lineHeight + 1;
                    }

                    if (ctx.drawing)
                    {
                        DGL_Enable(DGL_TEXTURE_2D);
                        drawSmallText(buf,
                                      ctx.widgetTopLeft.x + ctx.origin.x,
                                      ctx.widgetTopLeft.y + ctx.origin.y,
                                      ctx.alpha);
                        DGL_Disable(DGL_TEXTURE_2D);
                    }

                    ctx.origin.x += width + BIND_GAP;
                }

                // Update dimensions as we go.
                ctx.size = ctx.size.max({ctx.origin.x, ctx.origin.y + lineHeight});
            });

        return (ctx.size).toVec2ui();
    }
};

InputBindingWidget::InputBindingWidget()
    : Widget()
    , binds(nullptr)
    , d(new Impl(this))
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
}

InputBindingWidget::~InputBindingWidget()
{}

void InputBindingWidget::draw() const
{
    d->measureAndDraw(true);
}

int InputBindingWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_DELETE:
        S_LocalSound(SFX_MENU_CANCEL, NULL);
        d->iterateBindings(0, [](bindingitertype_t, int bid, const char *, dd_bool) {
            DD_Executef(true, "delbind %i", bid);
        });
        d->needGeometry = true;
        // If deleting the menuselect binding, automatically rebind it Return;
        // otherwise the user would be stuck without a way to make further bindings.
        if (binds->command && !strcmp(binds->command, "menuselect"))
        {
            DD_Execute(true, "bindevent menu:key-return menuselect");
        }
        return true;

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
    if (d->needGeometry)
    {
        d->needGeometry = false;
        geometry().setSize(d->measureAndDraw(false /* just measure */));
        geometry().setWidth(d->maxWidth);
    }
}

/**
 * Read the symbolic descriptor from the given @a event.
 */
static String symbolicDescriptor(const event_t &event)
{
    if(event.type == EV_SYMBOLIC)
    {
#ifndef __64BIT__
        String symbol = (const char *) event.data1;
#else
        String symbol = (const char *) event.data_u64;
#endif
        if(symbol.beginsWith("echo-"))
        {
            return symbol.substr(BytePos(5));
        }
    }
    return ""; // No symbolic descriptor.
}

int InputBindingWidget::handleEvent_Privileged(const event_t &event)
{
    LOG_AS("InputBindingWidget");

    // Only handle events when active.
    if (!isActive()) return false;

    // We're only interested in events with an echoed, symbolic descriptor.
    String symbol = symbolicDescriptor(event);
    if (symbol.isEmpty()) return false;

    // We're only interested in button down events.
    if((symbol.beginsWith("key-")         ||
        symbol.beginsWith("joy-button")   ||
        symbol.beginsWith("mouse-left")   ||
        symbol.beginsWith("mouse-middle") ||
        symbol.beginsWith("mouse-right")) && !symbol.endsWith("-down"))
    {
        return false;
    }

    const String context = bindContext();

    // The Delete key in the Menu context is reserved for deleting bindings
    if((!context.compareWithCase("menu") || !context.compareWithCase("shortcut")) &&
       symbol.beginsWith("key-delete-down"))
    {
        return false;
    }

    String cmd;
    if (binds->command)
    {
        cmd = Stringf("bindevent {%s:%s%s} {%s}",
                             context.c_str(),
                             symbol.c_str(),
                             binds->flags & CCF_MULTIPLAYER ? " + multiplayer" : "",
                             binds->command);

        // Check for repeats.
        if ((binds->flags & CCF_REPEAT) && symbol.endsWith("-down"))
        {
            cmd += Stringf("; bindevent {%s:%s-repeat} {%s}",
                                  context.c_str(),
                                  symbol.left(symbol.sizeb() - 5).c_str(),
                                  binds->command);
        }
    }
    else if (binds->controlName)
    {
        String stateFlags;

        // Extract the symbolic key/button name (exclude the state part).
        const auto endOfName = symbol.indexOf("-", symbol.indexOf("-") + 1);
        if (!endOfName)
        {
            throw Error("InputBindingWidget::handleEvent_Privileged", "Invalid symbol:" + symbol);
        }
        const String name = symbol.left(endOfName);

        // Staged?
        if (binds->flags & CCF_STAGED)
        {
            // Staging is buttons.
            if (name.beginsWith("key-") || name.beginsWith("joy-button") ||
                name.beginsWith("mouse-left") || name.beginsWith("mouse-middle") ||
                name.beginsWith("mouse-right"))
            {
                stateFlags += "-staged";
            }
        }

        // Inverted?
        bool inv = (binds->flags & CCF_INVERSE) != 0;
        if (symbol.substr(endOfName).beginsWith("-neg"))
        {
            inv = !inv;
        }
        if (inv)
        {
            stateFlags += "-inverse";
        }

        cmd = Stringf("bindcontrol {%s} {%s%s%s}",
                             binds->controlName,
                             name.c_str(),
                             stateFlags.c_str(),
                             (binds->flags & CCF_SIDESTEP_MODIFIER) ? " + modifier-1-up" : "");

        if(binds->flags & CCF_SIDESTEP_MODIFIER)
        {
            cmd += Stringf("; bindcontrol sidestep {%s%s + modifier-1-down}",
                                  name.c_str(),
                                  stateFlags.c_str());
        }
    }

    LOGDEV_INPUT_MSG("PrivilegedResponder: ") << cmd;
    DD_Execute(true, cmd);

    // We've finished the grab.
    setFlags(Active, UnsetFlags);
    DD_SetInteger(DD_SYMBOLIC_ECHO, false);
    S_LocalSound(SFX_MENU_ACCEPT, nullptr);
    d->needGeometry = true;
    return true;
}

const char *InputBindingWidget::controlName() const
{
    DE_ASSERT(binds);
    // Map to a text definition?
    if (PTR2INT(binds->text) > 0 && PTR2INT(binds->text) < NUMTEXT)
    {
        return GET_TXT(PTR2INT(binds->text));
    }
    return binds->text;
}

String InputBindingWidget::bindContext() const
{
    DE_ASSERT(binds);
    return (binds->bindContext ? binds->bindContext : "game");
}

void InputBindingWidget::pageActivated()
{
    d->needGeometry = true;
}

} // namespace menu
} // namespace common
