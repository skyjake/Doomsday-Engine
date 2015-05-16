/** @file chatwidget.cpp  Specialized HudWidget for player messaging.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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
#include "hud/widgets/chatwidget.h"

#include <de/Log>
#include "d_net.h"
#include "hu_stuff.h"  // shiftXForm
#include "p_tick.h"

using namespace de;

static void ChatWidget_UpdateGeometry(ChatWidget *chat)
{
    DENG2_ASSERT(chat);
    chat->updateGeometry();
}

static void ChatWidget_Draw(ChatWidget *chat, Point2Raw const *offset)
{
    DENG2_ASSERT(chat);
    chat->draw(offset? Vector2i(offset->xy) : Vector2i());
}

DENG2_PIMPL(ChatWidget)
{
    bool active      = false;
    bool shiftDown   = false;
    dint destination = 0;      ///< Default: All players.

    String text;

    Instance(Public *i) : Base(i) {}

    static void playSentSound()
    {
#if __JDOOM__
        S_LocalSound((gameModeBits & GM_ANY_DOOM2)? SFX_RADIO : SFX_TINK, 0);
#elif __JDOOM64__
        S_LocalSound(SFX_RADIO, 0);
#endif
    }

    void sendMessage()
    {
        String const msg = self.messageAsText();
        if(msg.isEmpty()) return;

        if(destination == 0)
        {
            // Send the message to all other players.
            if(!IS_NETGAME)
            {
                // Send it locally.
                for(dint i = 0; i < MAXPLAYERS; ++i)
                {
                    D_NetMessageNoSound(i, msg.toUtf8().constData());
                }
            }
            else
            {
                String const cmd = String("chat %1").arg(msg);
                char buf[256]; M_StrCatQuoted(buf, cmd.toUtf8().constData(), 256);
                DD_Execute(false, buf);
            }
        }
        else
        {
            // Send to all on the same team (team = color).
            for(dint i = 0; i < MAXPLAYERS; ++i)
            {
                if(!players[i].plr->inGame) continue;
                if(destination != cfg.playerColor[i]) continue;

                if(!IS_NETGAME)
                {
                    // Send it locally.
                    D_NetMessageNoSound(i, msg.toUtf8().constData());
                }
                else
                {
                    String const cmd = String("chatnum %1 %2").arg(i).arg(msg);
                    char buf[256]; M_StrCatQuoted(buf, cmd.toUtf8().constData(), 256);
                    DD_Execute(false, buf);
                }
            }
        }

        playSentSound();
    }
};

ChatWidget::ChatWidget(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(ChatWidget_UpdateGeometry),
                function_cast<DrawFunc>(ChatWidget_Draw),
                player)
    , d(new Instance(this))
{}

ChatWidget::~ChatWidget()
{}

bool ChatWidget::isActive() const
{
    return d->active;
}

void ChatWidget::activate(bool yes)
{
    bool const oldActive = isActive();

    if(d->active)
    {
        if(!yes)
        {
            d->active = false;
        }
    }
    else if(yes)
    {
        setDestination(0);  /// @c 0= "global" (default)
        d->text.clear();
        d->active = true;
    }

    if(d->active != oldActive)
    {
        DD_Executef(true, "%s chat", d->active? "activatebcontext" : "deactivatebcontext");
    }
}

dint ChatWidget::destination() const
{
    return d->destination;
}

void ChatWidget::setDestination(dint newDestination)
{
    if(newDestination >= 0 && newDestination <= NUMTEAMS)
    {
        d->destination = newDestination;
        return;
    }
    /// @throw DestinationError  Invalid destination given.
    throw DestinationError("ChatWidget::setDestination", "Unknown destination #" + String::number(newDestination) + " (not changed)");
}

dint ChatWidget::handleEvent(event_t const &ev)
{
    if(!isActive())
        return false;

    if(ev.type != EV_KEY)
        return false;

    if(ev.data1 == DDKEY_RSHIFT)
    {
        d->shiftDown = (ev.state == EVS_DOWN || ev.state == EVS_REPEAT);
        return false;  // Never eaten.
    }

    if(!(ev.state == EVS_DOWN || ev.state == EVS_REPEAT))
        return false;

    if(ev.data1 == DDKEY_BACKSPACE)
    {
        d->text.truncate(d->text.length() - 1);
        return true;
    }

    //
    // Append character(s) to the input buffer.
    //
    dint const oldLength = d->text.length();
    dchar const ch = ev.data1;
    if(ch >= ' ' && ch <= 'z')
    {
        d->text.append(QChar::fromLatin1(d->shiftDown ? ::shiftXForm[dint( ch )] : ch));
    }
    return oldLength != d->text.length();
}

dint ChatWidget::handleMenuCommand(menucommand_e cmd)
{
    if(!isActive()) return false;

    switch(cmd)
    {
    case MCMD_SELECT:  // Send the message.
        if(!d->text.isEmpty())
        {
            d->sendMessage();
        }
        activate(false);
        return true;

    case MCMD_CLOSE:
    case MCMD_NAV_OUT: // Close chat.
        activate(false);
        return true;

    case MCMD_DELETE:
        d->text.truncate(d->text.length() - 1);
        return true;

    default: return false;
    }
}

void ChatWidget::messageClear()
{
    d->text.clear();
}

void ChatWidget::messageAppend(String const &str)
{
    d->text += str;
}

String ChatWidget::messageAsText() const
{
    return d->text;
}

void ChatWidget::draw(Vector2i const &offset) const
{
    dfloat const textOpacity = uiRendState->pageAlpha * cfg.common.hudColor[3];
    //dfloat const iconOpacity = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(!isActive()) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(cfg.common.msgScale, cfg.common.msgScale, 1);

    FR_SetFont(font());
    FR_SetColorAndAlpha(cfg.common.hudColor[0], cfg.common.hudColor[1], cfg.common.hudColor[2], textOpacity);

    Block const text       = messageAsText().toUtf8();
    dint const textWidth   = FR_TextWidth(text.constData());
    dint const cursorWidth = FR_CharWidth('_');

    dint xOffset = 0;
    if(cfg.common.msgAlign == 1)
        xOffset = -(textWidth + cursorWidth)/2;
    else if(cfg.common.msgAlign == 2)
        xOffset = -(textWidth + cursorWidth);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawTextXY(text.constData(), xOffset, 0);
    if(actualMapTime & 12)
    {
        FR_DrawCharXY('_', xOffset + textWidth, 0);
    }
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ChatWidget::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!isActive()) return;

    FR_SetFont(font());

    Block const text = messageAsText().toUtf8();
    Size2Raw cursorSize; FR_CharSize(&cursorSize, '_');
    Size2Raw textSize;   FR_TextSize(&textSize, text.constData());
    Rect_SetWidthHeight(&geometry(), cfg.common.msgScale * (textSize.width + cursorSize.width),
                                     cfg.common.msgScale * (de::max(textSize.height, cursorSize.height)));
}

void ChatWidget::loadMacros()  // static
{
    // Retrieve the chat macro strings if not already set.
    for(dint i = 0; i < 10; ++i)
    {
        if(cfg.common.chatMacros[i]) continue;
        cfg.common.chatMacros[i] = (char *) GET_TXT(TXT_HUSTR_CHATMACRO0 + i);
    }
}

String ChatWidget::findMacro(dint macroId)  // static
{
    if(macroId >= 0 && macroId < 10)
    {
        return String(cfg.common.chatMacros[macroId]);
    }
    return "";
}

void ChatWidget::consoleRegister()
{
    C_VAR_BYTE("chat-beep", &cfg.common.chatBeep, 0, 0, 1);
    // User-configurable macros.
    for(dint i = 0; i < 10; ++i)
    {
        Block const cvarname = (String("chat-macro%1").arg(i)).toUtf8();
        C_VAR_CHARPTR(cvarname.constData(), &cfg.common.chatMacros[i], 0, 0, 0);
    }
}
