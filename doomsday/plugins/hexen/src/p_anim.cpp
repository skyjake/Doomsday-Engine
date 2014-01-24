/** @file p_anim.cpp  Parser for Hexen ANIMDEFS.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#include "jhexen.h"
#include "p_anim.h"

#include "hexlex.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"

static char const *textureScheme(Str const *string)
{
    struct KnownScheme {
        char const *label;
        char const *scheme;
    } knownSchemes[] = {
        { "texture", "Textures" },
        { "flat", "Flats" },
        { 0, 0 }
    };
    for(int i = 0; knownSchemes[i].label; ++i)
    {
        if(!Str_CompareIgnoreCase(string, knownSchemes[i].label))
        {
            return knownSchemes[i].scheme;
        }
    }
    return 0;
}

void AnimDefsParser(Str const *path)
{
    dd_bool isCustom;
    AutoStr *script = M_ReadFileIntoString(path, &isCustom);

    if(!script || Str_IsEmpty(script))
        return;

    App_Log(DE2_RES_VERBOSE, "Parsing \"%s\"...", F_PrettyPath(Str_Text(path)));

    HexLex lexer(script, path);

    while(lexer.readToken())
    {
        // string(texture-scheme) string(texture-path)
        if(char const *scheme = textureScheme(lexer.token()))
        {
            Uri *uri = lexer.readTextureUri(scheme);
            int const texNumBase = Textures_UniqueId2(uri, !isCustom);
            Uri_Delete(uri);

            bool const ignore = (texNumBase == -1);
            int groupNumber = 0;
            if(!ignore)
            {
                groupNumber = R_CreateAnimGroup(AGF_SMOOTH | AGF_FIRST_ONLY);
            }

            while(lexer.readToken())
            {
                if(!Str_CompareIgnoreCase(lexer.token(), "pic"))
                {
                    int picNum = lexer.readNumber();

                    int min = 0, max = 0;
                    Str const *label = lexer.readString();
                    if(!Str_CompareIgnoreCase(label, "tics"))
                    {
                        min = lexer.readNumber();
                    }
                    else if(!Str_CompareIgnoreCase(label, "rand"))
                    {
                        min = lexer.readNumber();
                        max = lexer.readNumber();
                    }
                    else
                    {
                        // Found an unexpected token.
                        Con_Error("AnimDefsParser: Unexpected token '%s' in \"%s\" on line #%i",
                                  lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
                    }

                    if(!ignore)
                    {
                        Uri *frameUrn = Uri_NewWithPath2("urn:", RC_NULL);

                        ddstring_t framePath; Str_InitStd(&framePath);
                        Str_Appendf(&framePath, "%s:%i", scheme, texNumBase + picNum - 1);

                        Uri_SetPath(frameUrn, Str_Text(&framePath));

                        R_AddAnimGroupFrame(groupNumber, frameUrn, min, (max > 0? max - min : 0));

                        Str_Free(&framePath);
                        Uri_Delete(frameUrn);
                    }

                    continue;
                }

                lexer.unreadToken();
                break;
            }

            continue;
        }

        // Found an unexpected token.
        Con_Error("AnimDefsParser: Unexpected token '%s' in \"%s\" on line #%i",
                  lexer.token(), F_PrettyPath(Str_Text(path)), lexer.lineNumber());
    }
}
