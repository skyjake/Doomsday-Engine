/** @file knownword.cpp
 * @ingroup console
 *
 * @todo Rewrite this whole thing with sensible C++ data structures and de::String! -jk
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/console/knownword.h"
#include "doomsday/console/exec.h"
#include "doomsday/console/var.h"
#include "doomsday/console/alias.h"
#include "doomsday/console/cmd.h"
#include "doomsday/help.h"
#include "doomsday/game.h"
#include <de/legacy/memory.h>
#include <de/c_wrapper.h>
#include <de/legacy/strutil.h>
#include <de/list.h>
#include <de/regexp.h>

using namespace de;

/// The list of known words (for completion).
/// @todo Replace with a persistent self-balancing BST (Treap?)?
static List<knownword_t> knownWords;
static dd_bool knownWordsNeedUpdate;

static void (*appWordsCallback)();

void Con_ClearKnownWords(void)
{
    knownWords.clear();
    knownWordsNeedUpdate = false;
}

/// Returns @c true, if a < b.
static bool compareKnownWordByName(const knownword_t &a, const knownword_t &b)
{
    const knownword_t *wA = &a;
    const knownword_t *wB = &b;
    AutoStr *textA = 0, *textB = 0;

    switch (wA->type)
    {
    case WT_CALIAS:   textA = AutoStr_FromTextStd(((calias_t *)wA->data)->name); break;
    case WT_CCMD:     textA = AutoStr_FromTextStd(((ccmd_t *)wA->data)->name); break;
    case WT_CVAR:     textA = CVar_ComposePath((cvar_t *)wA->data); break;
    case WT_GAME:     textA = AutoStr_FromTextStd(reinterpret_cast<const Game *>(wA->data)->id()); break;

    default:
        DE_ASSERT_FAIL("compareKnownWordByName: Invalid type for word A");
        return false;
    }

    switch (wB->type)
    {
    case WT_CALIAS:   textB = AutoStr_FromTextStd(((calias_t *)wB->data)->name); break;
    case WT_CCMD:     textB = AutoStr_FromTextStd(((ccmd_t *)wB->data)->name); break;
    case WT_CVAR:     textB = CVar_ComposePath((cvar_t *)wB->data); break;
    case WT_GAME:     textB = AutoStr_FromTextStd(reinterpret_cast<const Game *>(wB->data)->id()); break;

    default:
        DE_ASSERT_FAIL("compareKnownWordByName: Invalid type for word B");
        return false;
    }

    return Str_CompareIgnoreCase(textA, Str_Text(textB)) < 0;
}

/**
 * @return New AutoStr with the text of the known word. Caller gets ownership.
 */
static AutoStr *textForKnownWord(const knownword_t *word)
{
    AutoStr *text = 0;

    switch (word->type)
    {
    case WT_CALIAS:   text = AutoStr_FromTextStd(((calias_t *)word->data)->name); break;
    case WT_CCMD:     text = AutoStr_FromTextStd(((ccmd_t *)word->data)->name); break;
    case WT_CVAR:     text = CVar_ComposePath((cvar_t *)word->data); break;
    case WT_GAME:     text = AutoStr_FromTextStd(reinterpret_cast<const Game *>(word->data)->id()); break;

    default:
        DE_ASSERT_FAIL("textForKnownWord: Invalid type for word");
        text = AutoStr_FromTextStd("");
    }

    return text;
}

#if 0
static dd_bool removeFromKnownWords(knownwordtype_t type, void* data)
{
    DE_ASSERT(VALID_KNOWNWORDTYPE(type) && data != 0);

    for (int i = 0; i < knownWords.size(); ++i)
    {
        const knownword_t &word = knownWords.at(i);

        if (word.type != type || word.data != data)
            continue;

        knownWords.removeAt(i);
        return true;
    }
    return false;
}
#endif

void Con_AddKnownWord(knownwordtype_t type, void *ptr)
{
    knownword_t word;
    word.type = type;
    word.data = ptr;
    knownWords << word;
}

void Con_UpdateKnownWords()
{
    knownWordsNeedUpdate = true;
}

/**
 * Collate all known words and sort them alphabetically.
 * Commands, variables (except those hidden) and aliases are known words.
 */
static void updateKnownWords(void)
{
    if (!knownWordsNeedUpdate) return;

    /*
    // Count the number of visible console variables.
    countvariableparams_t countCVarParams;
    countCVarParams.count = 0;
    countCVarParams.type = cvartype_t(-1);
    countCVarParams.hidden = false;
    countCVarParams.ignoreHidden = true;
    if (cvarDirectory)
    {
        cvarDirectory->traverse(PathTree::NoBranch, NULL, CVarDirectory::no_hash, countVariable, &countCVarParams);
    }*/

    // Build the known words table.
    /*numKnownWords =
            numUniqueNamedCCmds +
            countCVarParams.count +
            numCAliases +
            App_Games().count();
    size_t len = sizeof(knownword_t) * numKnownWords;
    knownWords = (knownword_t*) M_Realloc(knownWords, len);
    memset(knownWords, 0, len);
*/
  //  uint knownWordIdx = 0;
    knownWords.clear();

    // Add commands?
    Con_AddKnownWordsForCommands();

    // Add variables?
    Con_AddKnownWordsForVariables();

    // Add aliases?
    Con_AddKnownWordsForAliases();

    if (appWordsCallback)
    {
        appWordsCallback();
    }

    // Sort it so we get nice alphabetical word completions.
    std::sort(knownWords.begin(), knownWords.end(), compareKnownWordByName);

    knownWordsNeedUpdate = false;
}

AutoStr *Con_KnownWordToString(const knownword_t *word)
{
    return textForKnownWord(word);
}

int Con_IterateKnownWords(const char *pattern, knownwordtype_t type,
                          int (*callback)(const knownword_t *word, void *parameters),
                          void *parameters)
{
    return Con_IterateKnownWords(KnownWordStartsWith, pattern, type, callback, parameters);
}

int Con_IterateKnownWords(KnownWordMatchMode matchMode,
                          char const* pattern, knownwordtype_t type,
                          int (*callback)(knownword_t const* word, void* parameters),
                          void* parameters)
{
    DE_ASSERT(callback);

    knownwordtype_t matchType = (VALID_KNOWNWORDTYPE(type)? type : WT_ANY);
    size_t patternLength = (pattern? strlen(pattern) : 0);
    int result = 0;
    const RegExp regex(matchMode == KnownWordRegex? pattern : "", CaseInsensitive);

    updateKnownWords();

    for (int i = 0; i < knownWords.sizei(); ++i)
    {
        const knownword_t *word = &knownWords.at(i);
        if (matchType != WT_ANY && word->type != type) continue;

        if (patternLength)
        {
            AutoStr* textString = textForKnownWord(word);
            if (matchMode == KnownWordStartsWith)
            {
                if (iCmpStrNCase(Str_Text(textString), pattern, patternLength))
                    continue; // Didn't match.
            }
            else if (matchMode == KnownWordExactMatch)
            {
                if (iCmpStrCase(Str_Text(textString), pattern))
                    continue; // Didn't match.
            }
            else if (matchMode == KnownWordRegex)
            {
                if (!regex.exactMatch(Str_Text(textString)))
                    continue; // Didn't match.
            }
        }

        result = callback(word, parameters);
        if (result) break;
    }

    return result;
}

static int countMatchedWordWorker(knownword_t const* /*word*/, void* parameters)
{
    DE_ASSERT(parameters);
    uint* count = (uint*) parameters;
    ++(*count);
    return 0; // Continue iteration.
}

typedef struct {
    knownword_t const** matches; /// Matched word array.
    uint index; /// Current position in the collection.
} collectmatchedwordworker_paramaters_t;

static int collectMatchedWordWorker(knownword_t const* word, void* parameters)
{
    DE_ASSERT(word && parameters);
    collectmatchedwordworker_paramaters_t* p = (collectmatchedwordworker_paramaters_t*) parameters;
    p->matches[p->index++] = word;
    return 0; // Continue iteration.
}

knownword_t const** Con_CollectKnownWordsMatchingWord(char const* word,
    knownwordtype_t type, uint* count)
{
    uint localCount = 0;
    Con_IterateKnownWords(word, type, countMatchedWordWorker, &localCount);

    if (count) *count = localCount;

    if (localCount != 0)
    {
        // Collect the pointers.
        collectmatchedwordworker_paramaters_t p;
        p.matches = (knownword_t const**) M_Malloc(sizeof(*p.matches) * (localCount + 1));
        p.index = 0;

        Con_IterateKnownWords(word, type, collectMatchedWordWorker, &p);
        p.matches[localCount] = 0; // Terminate.

        return p.matches;
    }

    return 0; // No matches.
}

static int aproposPrinter(const knownword_t *word, void *matching)
{
    AutoStr *text = textForKnownWord(word);

    // See if 'matching' is anywhere in the known word.
    if (strcasestr(Str_Text(text), (const char*)matching))
    {
        char const* wType[KNOWNWORDTYPE_COUNT] = {
            "cmd ", "var ", "alias ", "game "
        };

        std::ostringstream os;

        os << _E(l) << wType[word->type]
           << _E(0) << _E(b) << Str_Text(text) << " " << _E(2) << _E(>);

        // Look for a short description.
        String tmp;
        if (word->type == WT_CCMD || word->type == WT_CVAR)
        {
            const char *desc = DH_GetString(DH_Find(Str_Text(text)), HST_DESCRIPTION);
            if (desc)
            {
                tmp = desc;
            }
        }
        else if (word->type == WT_GAME)
        {
            tmp = reinterpret_cast<const Game *>(word->data)->title();
        }

        os << tmp;

        LOG_SCR_MSG("%s") << os.str();
    }

    return 0;
}

static void printApropos(const char *matching)
{
    /// @todo  Extend the search to cover the contents of all help strings (dd_help.c).
    Con_IterateKnownWords(0, WT_ANY, aproposPrinter, (void *)matching);
}

D_CMD(HelpApropos)
{
    DE_UNUSED(argc, src);

    printApropos(argv[1]);
    return true;
}

struct AnnotationWork
{
    de::Set<de::String> terms;
    de::String result;
};

static int annotateMatchedWordCallback(const knownword_t *word, void *parameters)
{
    AnnotationWork *work = reinterpret_cast<AnnotationWork *>(parameters);
    AutoStr *name = Con_KnownWordToString(word);
    de::String found;

    if (!work->terms.contains(Str_Text(name)))
        return false; // keep going

    switch (word->type)
    {
    case WT_CVAR:
        if (!(((cvar_t *)word->data)->flags & CVF_HIDE))
        {
            found = Con_VarAsStyledText((cvar_t *) word->data, "");
        }
        break;

    case WT_CCMD:
        if (!((ccmd_t *)word->data)->prevOverload)
        {
            found = Con_CmdAsStyledText((ccmd_t *) word->data);
        }
        break;

    case WT_CALIAS:
        found = Con_AliasAsStyledText((calias_t *) word->data);
        break;

    case WT_GAME:
        found = Con_GameAsStyledText(reinterpret_cast<const Game *>(word->data));
        break;

    default:
        break;
    }

    if (!found.isEmpty())
    {
        if (!work->result.isEmpty()) work->result.append("\n");
        work->result.append(found);
    }

    return false; // don't stop
}

de::String Con_AnnotatedConsoleTerms(const StringList &terms)
{
    AnnotationWork work;
    for (const String &term : terms)
    {
        work.terms.insert(term);
    }
    Con_IterateKnownWords(nullptr, WT_ANY, annotateMatchedWordCallback, &work);
    return work.result;
}

static int addToTerms(const knownword_t *word, void *parameters)
{
    Lexicon *lexi = reinterpret_cast<Lexicon *>(parameters);
    lexi->addTerm(Str_Text(Con_KnownWordToString(word)));
    return 0;
}

Lexicon Con_Lexicon()
{
    Lexicon lexi;
    Con_IterateKnownWords(nullptr, WT_ANY, addToTerms, &lexi);
    lexi.setAdditionalWordChars("-_.");
    return lexi;
}

void Con_SetApplicationKnownWordCallback(void (*callback)())
{
    appWordsCallback = callback;
}

static int addToStringList(const knownword_t *word, void *parameters)
{
    StringList *terms = reinterpret_cast<StringList *>(parameters);
    terms->append(Str_Text(Con_KnownWordToString(word)));
    return 0;
}

void Con_TermsRegex(StringList &terms, const String &pattern, knownwordtype_t wordType)
{
    terms.clear();
    Con_IterateKnownWords(KnownWordRegex, pattern, wordType,
                          addToStringList, &terms);
}
