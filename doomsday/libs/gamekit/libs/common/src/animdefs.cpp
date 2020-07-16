/** @file animdef.cpp  ANIMATED and ANIMDEFS -defined texture animation interpreters.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 * @authors Copyright © 1993-1996 by id Software, Inc.
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
#include "animdefs.h"

#include <de/log.h>
#include <de/nativepath.h>
#if __JHEXEN__
#  include "hexlex.h"
#endif

using namespace de;
using namespace res;

#if !__JHEXEN__

/**
 * Texture animation definition flags.
 */
enum {
    /// The animation belongs to the "Textures" resource scheme, rather than "Flats".
    TexturesScheme   = 0x1,

    /// Disable drawing of decals on world surfaces whose material is derived from
    /// this texture animation. Not currently supported. (A ZDoom extension).
    NoDecals         = 0x2,

    DefaultAnimFlags = 0 // In the "Flats" resource scheme
};

/**
 * ANIMATED format, texture animation definition.
 *
 * In Doomsday these are handled via DED definitions.
 * In BOOM they invented the ANIMATED lump for the same purpose.
 *
 * @note This struct is read directly from the lump, so it must be it aligned.
 */
#pragma pack(1)
struct TextureAnimDef
{
    signed char flags;
    char endname[9];
    char startname[9];

    // Frame duration in tics.
    // Note that a value of 65536 or greater should be interpreted as a "swirly texture"
    // animation and expects a single frame. Not currently supported. (A SMMU extension).
    int speed;
};
#pragma pack()

// This array is treated as a hardcoded replacement for data that can be loaded from a
// lump, so we need to use little-endian byte ordering.
static TextureAnimDef animsShared[] = {
#if __JDOOM64__
    { TexturesScheme,   "CFACEC",   "CFACEA",   MACRO_LONG(4) },
    { TexturesScheme,   "SMONAD",   "SMONAA",   MACRO_LONG(8) },
    { TexturesScheme,   "SMONBD",   "SMONBA",   MACRO_LONG(2) },
    { TexturesScheme,   "SMONCD",   "SMONCA",   MACRO_LONG(8) },
    { TexturesScheme,   "SMONDD",   "SMONDA",   MACRO_LONG(4) },
    { TexturesScheme,   "SMONED",   "SMONEA",   MACRO_LONG(8) },
    { TexturesScheme,   "SPORTI",   "SPORTA",   MACRO_LONG(4) },
    { TexturesScheme,   "C307B",    "C307",     MACRO_LONG(8) },
#elif __JDOOM__
    // Doom anims:
    { DefaultAnimFlags, "BLOOD3",   "BLOOD1",   MACRO_LONG(8) },
    { DefaultAnimFlags, "FWATER4",  "FWATER1",  MACRO_LONG(8) },
    { DefaultAnimFlags, "SWATER4",  "SWATER1",  MACRO_LONG(8) },
    { DefaultAnimFlags, "LAVA4",    "LAVA1",    MACRO_LONG(8) },
    { DefaultAnimFlags, "NUKAGE3",  "NUKAGE1",  MACRO_LONG(8) },
    { TexturesScheme,   "BLODRIP4", "BLODRIP1", MACRO_LONG(8) },
    { TexturesScheme,   "FIREBLU2", "FIREBLU1", MACRO_LONG(8) },
    { TexturesScheme,   "FIRELAVA", "FIRELAV3", MACRO_LONG(8) },
    { TexturesScheme,   "FIREMAG3", "FIREMAG1", MACRO_LONG(8) },
    { TexturesScheme,   "FIREWALL", "FIREWALA", MACRO_LONG(8) },
    { TexturesScheme,   "GSTFONT3", "GSTFONT1", MACRO_LONG(8) },
    { TexturesScheme,   "ROCKRED3", "ROCKRED1", MACRO_LONG(8) },
    { TexturesScheme,   "SLADRIP3", "SLADRIP1", MACRO_LONG(8) },
    { TexturesScheme,   "WFALL4",   "WFALL1",   MACRO_LONG(8) },
    { TexturesScheme,   "BLODGR4",  "BLODGR1",  MACRO_LONG(8) },

    // Doom 2 anims:
    { DefaultAnimFlags, "RROCK08",  "RROCK05",  MACRO_LONG(8) },
    { DefaultAnimFlags, "SLIME04",  "SLIME01",  MACRO_LONG(8) },
    { DefaultAnimFlags, "SLIME08",  "SLIME05",  MACRO_LONG(8) },
    { DefaultAnimFlags, "SLIME12",  "SLIME09",  MACRO_LONG(8) },
    { TexturesScheme,   "BFALL4",   "BFALL1",   MACRO_LONG(8) },
    { TexturesScheme,   "DBRAIN4",  "DBRAIN1",  MACRO_LONG(8) },
    { TexturesScheme,   "SFALL4",   "SFALL1",   MACRO_LONG(8) },
#elif __JHERETIC__
    { DefaultAnimFlags, "FLTWAWA3", "FLTWAWA1", MACRO_LONG(8) },
    { DefaultAnimFlags, "FLTSLUD3", "FLTSLUD1", MACRO_LONG(8) },
    { DefaultAnimFlags, "FLTTELE4", "FLTTELE1", MACRO_LONG(6) },
    { DefaultAnimFlags, "FLTFLWW3", "FLTFLWW1", MACRO_LONG(9) },
    { DefaultAnimFlags, "FLTLAVA4", "FLTLAVA1", MACRO_LONG(8) },
    { DefaultAnimFlags, "FLATHUH4", "FLATHUH1", MACRO_LONG(8) },
    { TexturesScheme,   "LAVAFL3",  "LAVAFL1",  MACRO_LONG(6) },
    { TexturesScheme,   "WATRWAL3", "WATRWAL1", MACRO_LONG(4) },
#endif
    { -1, "\0", "\0", MACRO_LONG(0) }
};

static void loadAnimDefs(const TextureAnimDef *defs, bool customDefs)
{
    DE_ASSERT(defs != 0);

    LOG_AS("loadAnimDefs");

    AutoStr *framePath = AutoStr_NewStd();
    AutoStr *startPath = AutoStr_NewStd();
    AutoStr *endPath   = AutoStr_NewStd();

    uri_s *frameUrn = Uri_NewWithPath2("urn:", RC_NULL);
    uri_s *startUri = Uri_New();
    uri_s *endUri   = Uri_New();

    // Read structures until -1 is found
    bool lastIsTexture = false;
    for(int i = 0; defs[i].flags != -1; ++i)
    {
        const bool isTexture = (defs[i].flags & TexturesScheme) != 0;

        // Scheme change?
        if(i == 0 || isTexture != lastIsTexture)
        {
            Uri_SetScheme(startUri, isTexture? "Textures" : "Flats");
            Uri_SetScheme(endUri,   isTexture? "Textures" : "Flats");
            lastIsTexture = isTexture;
        }

        // Translate and encode the referenced texture names.
        Str_PercentEncode(Str_StripRight(Str_Set(startPath, defs[i].startname)));
        Uri_SetPath(startUri, Str_Text(startPath));

        Str_PercentEncode(Str_StripRight(Str_Set(endPath,   defs[i].endname)));
        Uri_SetPath(endUri, Str_Text(endPath));

        // Are both frames in the animation known?
        const int startFrame = Textures_UniqueId2(startUri, !customDefs);
        const int endFrame   = Textures_UniqueId2(endUri,   !customDefs);
        if(-1 == startFrame || -1 == endFrame) continue;

        // Is the defined range valid?
        const int numFrames = endFrame - startFrame + 1;
        if(numFrames < 2)
        {
            LOG_RES_WARNING("Bad cycle from '%s' to '%s' in sequence #%i")
                    << defs[i].startname << defs[i].endname << i;
            continue;
        }

        // Doomsday's group animation needs to know the texture/flat numbers of ALL frames
        // in the animation group so we'll have to step through the directory adding frames
        // as we go. (DOOM only required the start/end texture/flat numbers and would animate
        // all textures/flats inbetween).

        const int groupNum     = R_CreateAnimGroup(AGF_SMOOTH);
        const AutoStr *from    = Uri_ToString(startUri);
        const AutoStr *to      = Uri_ToString(endUri);
        const int ticsPerFrame = DD_LONG(defs[i].speed);

        LOG_RES_AT_LEVEL(customDefs? LogEntry::Verbose : LogEntry::XVerbose,
                         "  %d: From:\"%s\" To:\"%s\" Tics:%i")
                << i << Str_Text(from) << Str_Text(to) << ticsPerFrame;

        for(int n = startFrame; n <= endFrame; ++n)
        {
            Str_Clear(framePath);
            Str_Appendf(framePath, "%s:%i", isTexture? "Textures" : "Flats", n);
            Uri_SetPath(frameUrn, Str_Text(framePath));

            R_AddAnimGroupFrame(groupNum, frameUrn, ticsPerFrame, 0);
        }
    }

    Uri_Delete(endUri);
    Uri_Delete(startUri);
    Uri_Delete(frameUrn);
}
#endif // !__JHEXEN__

#if __JHEXEN__
static const char *textureScheme(const ddstring_s *string)
{
    struct KnownScheme {
        const char *label;
        const char *scheme;
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

/**
 * Attempt to parse the script on the identified @a path as "animation definition" data.
 */
static void AnimDefsParser(const ddstring_s *path)
{
    dd_bool isCustom;
    AutoStr *script = M_ReadFileIntoString(path, &isCustom);

    if(!script || Str_IsEmpty(script))
        return;

    LOG_RES_VERBOSE("Parsing \"%s\"...") << NativePath(Str_Text(path)).pretty();

    HexLex lexer(script, path);

    while(lexer.readToken())
    {
        // string(texture-scheme) string(texture-path)
        if(const char *scheme = textureScheme(lexer.token()))
        {
            res::Uri uri = lexer.readUri(scheme);
            const int texNumBase = Textures_UniqueId2(reinterpret_cast<uri_s *>(&uri), !isCustom);

            const bool ignore = (texNumBase == -1);
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
                    const ddstring_s *label = lexer.readString();
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
                        uri_s *frameUrn = Uri_NewWithPath2("urn:", RC_NULL);
                        Uri_SetPath(frameUrn, Str_Text(Str_Appendf(AutoStr_NewStd(), "%s:%i", scheme, texNumBase + picNum - 1)));

                        R_AddAnimGroupFrame(groupNumber, frameUrn, min, (max > 0? max - min : 0));

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
#endif // __JHEXEN__

void P_InitPicAnims()
{
#if __JHEXEN__
    AnimDefsParser(AutoStr_FromText("Lumps:ANIMDEFS"));
#else
    if(CentralLumpIndex().contains("ANIMATED.lmp"))
    {
        File1 &lump = CentralLumpIndex()[CentralLumpIndex().findLast("ANIMATED.lmp")];

        // Support this BOOM extension by reading the data and then registering
        // the new animations into Doomsday using the animation groups feature.
        //
        // Support for this extension should be considered deprecated.
        // All new features should be added, accessed via DED.
        LOG_RES_VERBOSE("Processing lump %s::ANIMATED")
                << NativePath(lump.container().composePath()).pretty();

        loadAnimDefs((TextureAnimDef *)lump.cache(), true);
        lump.unlock();
        return;
    }

    LOGDEV_RES_VERBOSE("Registering default texture animations...");
    loadAnimDefs(animsShared, false);
#endif
}
