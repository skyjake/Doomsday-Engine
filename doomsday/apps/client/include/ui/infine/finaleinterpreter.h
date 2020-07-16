/** @file finaleinterpreter.h  InFine animation system Finale script interpreter.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_UI_INFINE_FINALEINTERPRETER_H
#define DE_UI_INFINE_FINALEINTERPRETER_H

#include <de/error.h>
#include <de/string.h>
#include "../ddevent.h"
#include "api_infine.h"  // finaleid_t

class FinaleWidget;
class FinaleAnimWidget;
class FinaleTextWidget;
class FinalePageWidget;

/// Used with findWidget and findOrCreateWidget:
/// @ingroup infine
enum fi_obtype_e
{
    FI_ANIM,
    FI_TEXT
};

/**
 * Interpreter for finale scripts. An instance of which is created for each running
 * script and owned by the Finale.
 *
 * @par UI pages / drawing order
 * InFine imposes a strict object drawing order, which requires two pages; one for
 * animation objects (also used for the background) and another for Text objects
 * (also used for the filter).
 *
 * 1: Background.
 * 2: Picture objects in the order in which they were created.
 * 3: Text objects, in the order in which they were created.
 * 4: Filter.
 *
 * @see Finale
 * @ingroup infine
 */
class FinaleInterpreter
{
public:
    /// An unknown widget was referenced. @ingroup errors
    DE_ERROR(MissingWidgetError);

    /// An unknown page was referenced. @ingroup errors
    DE_ERROR(MissingPageError);

    enum PageIndex
    {
        Anims = 0,  ///< @note Also used for the background.
        Texts = 1   ///< @note Also used for the filter.
    };

public:
    FinaleInterpreter(finaleid_t id);

    finaleid_t id() const;

    bool runTicks(timespan_t timeDelta, bool processCommands);
    int handleEvent(const ddevent_t &ev);

    void loadScript(const char *script);

    bool isSuspended() const;

    void resume();
    void suspend();
    void terminate();

    bool isMenuTrigger() const;
    bool commandExecuted() const;

    bool canSkip() const;
    void allowSkip(bool yes = true);

    bool skip();
    bool skipToMarker(const de::String &marker);
    bool skipInProgress() const;
    bool lastSkipped() const;

#ifdef __CLIENT__
    void addEventHandler(const ddevent_t &evTemplate, const de::String &gotoMarker);
    void removeEventHandler(const ddevent_t &evTemplate);
#endif

    FinalePageWidget &page(PageIndex index);
    const FinalePageWidget &page(PageIndex index) const;

    FinaleWidget *tryFindWidget(const de::String &name);

    FinaleWidget &findWidget(fi_obtype_e type, const de::String &name);

    /**
     * Find an object of the specified type with the type-unique name.
     *
     * @param type  FinaleWidget type identifier.
     * @param name  Unique name of the object we are looking for.
     *
     * @return  a) Existing object associated with unique @a name.
     *          b) New object with unique @a name.
     */
    FinaleWidget &findOrCreateWidget(fi_obtype_e type, const de::String &name);

public: /// Script-level flow/state control (@todo make private): --------------------

    void beginDoSkipMode();
    void gotoEnd();
    void pause();
    void wait(int ticksToWait = 1);
    void foundSkipHere();
    void foundSkipMarker(const de::String &marker);

    int inTime() const;
    void setInTime(int seconds);

    void setHandleEvents(bool yes = true);
    void setShowMenu(bool yes = true);
    void setSkip(bool allowed = true);
    void setSkipNext(bool yes = true);
    void setWaitAnim(FinaleAnimWidget *newWaitAnim);
    void setWaitText(FinaleTextWidget *newWaitText);

private:
    DE_PRIVATE(d)
};

#endif // DE_UI_INFINE_FINALEINTERPRETER_H
