/** @file knownword.h
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_CONSOLE_KNOWNWORD_H
#define LIBDOOMSDAY_CONSOLE_KNOWNWORD_H

#include "../libdoomsday.h"
#include "../game.h"
#include "dd_share.h"
#include "dd_types.h"
#include <de/lexicon.h>

typedef enum {
    WT_ANY = -1,
    KNOWNWORDTYPE_FIRST = 0,
    WT_CCMD = KNOWNWORDTYPE_FIRST,
    WT_CVAR,
    WT_CALIAS,
    WT_GAME,
    KNOWNWORDTYPE_COUNT
} knownwordtype_t;

#define VALID_KNOWNWORDTYPE(t)      ((t) >= KNOWNWORDTYPE_FIRST && (t) < KNOWNWORDTYPE_COUNT)

typedef struct knownword_s {
    knownwordtype_t type;
    void *data;
} knownword_t;

void Con_DataRegister();

void Con_UpdateKnownWords();

void Con_ClearKnownWords();

/**
 * Sets a callback that is called whenever the set of known words needs updating.
 * The callback should add all the known words using Con_AddKnownWord.
 *
 * @param callback  Known word addition callback.
 */
LIBDOOMSDAY_PUBLIC void Con_SetApplicationKnownWordCallback(void (*callback)());

LIBDOOMSDAY_PUBLIC void Con_AddKnownWord(knownwordtype_t type, void *ptr);

/**
 * Iterate over words in the known-word dictionary, making a callback for each.
 * Iteration ends when all selected words have been visited or a callback returns
 * non-zero.
 *
 * @param pattern           If not @c NULL or an empty string, only process those
 *                          words which match this pattern.
 * @param type              If a valid word type, only process words of this type.
 * @param callback          Callback to make for each processed word.
 * @param parameters        Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
LIBDOOMSDAY_PUBLIC int Con_IterateKnownWords(const char *pattern, knownwordtype_t type,
    int (*callback)(const knownword_t *word, void *parameters), void *parameters);

enum KnownWordMatchMode {
    KnownWordExactMatch,  // case insensitive
    KnownWordStartsWith,  // case insensitive
    KnownWordRegex        // case insensitive
};

LIBDOOMSDAY_PUBLIC int Con_IterateKnownWords(KnownWordMatchMode matchMode, const char *pattern,
    knownwordtype_t type, int (*callback)(const knownword_t *word, void *parameters),
    void *parameters);

/**
 * Collect an array of knownWords which match the given word (at least
 * partially).
 *
 * \note: The array must be freed with free()
 *
 * @param word              The word to be matched.
 * @param type              If a valid word type, only collect words of this type.
 * @param count             If not @c NULL the matched word count is written back here.
 *
 * @return  A NULL-terminated array of pointers to all the known words which
 *          match the search criteria.
 */
LIBDOOMSDAY_PUBLIC const knownword_t **Con_CollectKnownWordsMatchingWord(const char *word,
    knownwordtype_t type, uint *count);

LIBDOOMSDAY_PUBLIC AutoStr *Con_KnownWordToString(const knownword_t *word);

LIBDOOMSDAY_PUBLIC de::String Con_AnnotatedConsoleTerms(const de::StringList &terms);

/**
 * Collects all the known words of the console into a Lexicon.
 */
LIBDOOMSDAY_PUBLIC de::Lexicon Con_Lexicon();

LIBDOOMSDAY_PUBLIC void Con_TermsRegex(de::StringList &terms, const de::String &pattern,
                                       knownwordtype_t wordType);

#endif // LIBDOOMSDAY_CONSOLE_KNOWNWORD_H
