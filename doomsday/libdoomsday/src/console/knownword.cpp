/** @file knownword.cpp
 * @ingroup console
 *
 * @todo Rewrite this whole thing with sensible C++ data structures and de::String! -jk
 *
 * @authors Copyright &copy; 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/memory.h>
#include <de/game/Game>
#include <de/c_wrapper.h>
#include <QList>

using namespace de;

/// The list of known words (for completion).
/// @todo Replace with a persistent self-balancing BST (Treap?)?
static QList<knownword_t> knownWords;
static dd_bool knownWordsNeedUpdate;

void Con_ClearKnownWords(void)
{
    knownWords.clear();
    knownWordsNeedUpdate = false;
}

/// Returns @c true, if a < b.
static bool compareKnownWordByName(knownword_t const &a, knownword_t const &b)
{
    knownword_t const *wA = &a;
    knownword_t const *wB = &b;
    AutoStr *textA = 0, *textB = 0;

    switch(wA->type)
    {
    case WT_CALIAS:   textA = AutoStr_FromTextStd(((calias_t *)wA->data)->name); break;
    case WT_CCMD:     textA = AutoStr_FromTextStd(((ccmd_t *)wA->data)->name); break;
    case WT_CVAR:     textA = CVar_ComposePath((cvar_t *)wA->data); break;
    case WT_GAME:     textA = AutoStr_FromTextStd(reinterpret_cast<game::Game const *>(wA->data)->id().toUtf8().constData()); break;

    default:
        App_FatalError("compareKnownWordByName: Invalid type %i for word A.", wA->type);
        exit(1); // Unreachable
    }

    switch(wB->type)
    {
    case WT_CALIAS:   textB = AutoStr_FromTextStd(((calias_t *)wB->data)->name); break;
    case WT_CCMD:     textB = AutoStr_FromTextStd(((ccmd_t *)wB->data)->name); break;
    case WT_CVAR:     textB = CVar_ComposePath((cvar_t *)wB->data); break;
    case WT_GAME:     textB = AutoStr_FromTextStd(reinterpret_cast<game::Game const *>(wB->data)->id().toUtf8().constData()); break;

    default:
        App_FatalError("compareKnownWordByName: Invalid type %i for word B.", wB->type);
        exit(1); // Unreachable
    }

    return Str_CompareIgnoreCase(textA, Str_Text(textB)) < 0;
}

/**
 * @return New AutoStr with the text of the known word. Caller gets ownership.
 */
static AutoStr *textForKnownWord(knownword_t const *word)
{
    AutoStr *text = 0;

    switch(word->type)
    {
    case WT_CALIAS:   text = AutoStr_FromTextStd(((calias_t *)word->data)->name); break;
    case WT_CCMD:     text = AutoStr_FromTextStd(((ccmd_t *)word->data)->name); break;
    case WT_CVAR:     text = CVar_ComposePath((cvar_t *)word->data); break;
    case WT_GAME:     text = AutoStr_FromTextStd(reinterpret_cast<game::Game const *>(word->data)->id().toUtf8().constData()); break;

    default:
        App_FatalError("textForKnownWord: Invalid type %i for word.", word->type);
        exit(1); // Unreachable
    }

    return text;
}

#if 0
static dd_bool removeFromKnownWords(knownwordtype_t type, void* data)
{
    DENG_ASSERT(VALID_KNOWNWORDTYPE(type) && data != 0);

    for(int i = 0; i < knownWords.size(); ++i)
    {
        knownword_t const &word = knownWords.at(i);

        if(word.type != type || word.data != data)
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
    if(!knownWordsNeedUpdate) return;

    /*
    // Count the number of visible console variables.
    countvariableparams_t countCVarParams;
    countCVarParams.count = 0;
    countCVarParams.type = cvartype_t(-1);
    countCVarParams.hidden = false;
    countCVarParams.ignoreHidden = true;
    if(cvarDirectory)
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

    /// @todo Add a callback for app-specific known words. -jk

#if 0 // we don't have access to the games list!
    // Add games?
    foreach(Game *game, App_Games().all())
    {
        knownWords[knownWordIdx].type = WT_GAME;
        knownWords[knownWordIdx].data = game;
        ++knownWordIdx;
    }
#endif

    // Sort it so we get nice alphabetical word completions.
    qSort(knownWords.begin(), knownWords.end(), compareKnownWordByName);

    knownWordsNeedUpdate = false;
}

AutoStr *Con_KnownWordToString(knownword_t const *word)
{
    return textForKnownWord(word);
}

int Con_IterateKnownWords(char const *pattern, knownwordtype_t type,
                          int (*callback)(knownword_t const *word, void *parameters),
                          void *parameters)
{
    return Con_IterateKnownWords(KnownWordStartsWith, pattern, type, callback, parameters);
}

int Con_IterateKnownWords(KnownWordMatchMode matchMode,
                          char const* pattern, knownwordtype_t type,
                          int (*callback)(knownword_t const* word, void* parameters),
                          void* parameters)
{
    DENG_ASSERT(callback);

    knownwordtype_t matchType = (VALID_KNOWNWORDTYPE(type)? type : WT_ANY);
    size_t patternLength = (pattern? strlen(pattern) : 0);
    int result = 0;

    updateKnownWords();

    for(int i = 0; i < knownWords.size(); ++i)
    {
        knownword_t const *word = &knownWords.at(i);
        if(matchType != WT_ANY && word->type != type) continue;

        if(patternLength)
        {
            AutoStr* textString = textForKnownWord(word);
            if(matchMode == KnownWordStartsWith)
            {
                if(strnicmp(Str_Text(textString), pattern, patternLength))
                    continue; // Didn't match.
            }
            else if(matchMode == KnownWordExactMatch)
            {
                if(strcasecmp(Str_Text(textString), pattern))
                    continue; // Didn't match.
            }
        }

        result = callback(word, parameters);
        if(result) break;
    }

    return result;
}

static int countMatchedWordWorker(knownword_t const* /*word*/, void* parameters)
{
    DENG_ASSERT(parameters);
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
    DENG_ASSERT(word && parameters);
    collectmatchedwordworker_paramaters_t* p = (collectmatchedwordworker_paramaters_t*) parameters;
    p->matches[p->index++] = word;
    return 0; // Continue iteration.
}

knownword_t const** Con_CollectKnownWordsMatchingWord(char const* word,
    knownwordtype_t type, uint* count)
{
    uint localCount = 0;
    Con_IterateKnownWords(word, type, countMatchedWordWorker, &localCount);

    if(count) *count = localCount;

    if(localCount != 0)
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


static int aproposPrinter(knownword_t const *word, void *matching)
{
    AutoStr *text = textForKnownWord(word);

    // See if 'matching' is anywhere in the known word.
    if(strcasestr(Str_Text(text), (const char*)matching))
    {
        char const* wType[KNOWNWORDTYPE_COUNT] = {
            "cmd ", "var ", "alias ", "game "
        };

        String str;
        QTextStream os(&str);

        os << _E(l) << wType[word->type]
           << _E(0) << _E(b) << Str_Text(text) << " " << _E(2) << _E(>);

        // Look for a short description.
        String tmp;
        if(word->type == WT_CCMD || word->type == WT_CVAR)
        {
            char const *desc = DH_GetString(DH_Find(Str_Text(text)), HST_DESCRIPTION);
            if(desc)
            {
                tmp = desc;
            }
        }
        else if(word->type == WT_GAME)
        {
            tmp = reinterpret_cast<game::Game const *>(word->data)->title();
        }

        os << tmp;

        LOG_SCR_MSG("%s") << str;
    }

    return 0;
}

static void printApropos(char const *matching)
{
    /// @todo  Extend the search to cover the contents of all help strings (dd_help.c).
    Con_IterateKnownWords(0, WT_ANY, aproposPrinter, (void *)matching);
}

D_CMD(HelpApropos)
{
    DENG2_UNUSED2(argc, src);

    printApropos(argv[1]);
    return true;
}

struct AnnotationWork
{
    QSet<QString> terms;
    de::String result;
};

static int annotateMatchedWordCallback(knownword_t const *word, void *parameters)
{
    AnnotationWork *work = reinterpret_cast<AnnotationWork *>(parameters);
    AutoStr *name = Con_KnownWordToString(word);
    de::String found;

    if(!work->terms.contains(Str_Text(name)))
        return false; // keep going

    switch(word->type)
    {
    case WT_CVAR:
        if(!(((cvar_t *)word->data)->flags & CVF_HIDE))
        {
            found = Con_VarAsStyledText((cvar_t *) word->data, "");
        }
        break;

    case WT_CCMD:
        if(!((ccmd_t *)word->data)->prevOverload)
        {
            found = Con_CmdAsStyledText((ccmd_t *) word->data);
        }
        break;

    case WT_CALIAS:
        found = Con_AliasAsStyledText((calias_t *) word->data);
        break;

    case WT_GAME:
        found = Con_GameAsStyledText(reinterpret_cast<game::Game const *>(word->data));
        break;

    default:
        break;
    }

    if(!found.isEmpty())
    {
        if(!work->result.isEmpty()) work->result.append("\n");
        work->result.append(found);
    }

    return false; // don't stop
}

de::String Con_AnnotatedConsoleTerms(QStringList terms)
{
    AnnotationWork work;
    foreach(QString term, terms)
    {
        work.terms.insert(term);
    }
    Con_IterateKnownWords(NULL, WT_ANY, annotateMatchedWordCallback, &work);
    return work.result;
}

static int addToTerms(knownword_t const *word, void *parameters)
{
    shell::Lexicon *lexi = reinterpret_cast<shell::Lexicon *>(parameters);
    lexi->addTerm(Str_Text(Con_KnownWordToString(word)));
    return 0;
}

shell::Lexicon Con_Lexicon()
{
    shell::Lexicon lexi;
    Con_IterateKnownWords(0, WT_ANY, addToTerms, &lexi);
    lexi.setAdditionalWordChars("-_.");
    return lexi;
}
