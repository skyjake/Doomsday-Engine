/** @file finaleinterpreter.h  InFine animation system Finale script interpreter.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_UI_INFINE_FINALEINTERPRETER_H
#define DENG_UI_INFINE_FINALEINTERPRETER_H

#include "dd_input.h" // ddevent_t
#include "dd_ui.h"    // finaleid_t

struct fi_object_s;

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
 * @ingroup InFine
 */
class FinaleInterpreter
{
public:
    enum PageIndex
    {
        Anims = 0,  ///< @note Also used for the background.
        Texts = 1   ///< @note Also used for the filter.
    };

public:
    FinaleInterpreter(finaleid_t id);

    finaleid_t id() const;

    bool runTicks();
    int handleEvent(ddevent_t const &ev);

    void loadScript(char const *script);

    bool isSuspended() const;

    void resume();
    void suspend();
    void terminate();

    bool isMenuTrigger() const;
    bool commandExecuted() const;

    bool canSkip() const;
    void allowSkip(bool yes = true);

    bool skip();
    bool skipToMarker(char const *marker);
    bool skipInProgress() const;
    bool lastSkipped() const;

#ifdef __CLIENT__
    void addEventHandler(ddevent_t const &evTemplate, char const *gotoMarker);
    void removeEventHandler(ddevent_t const &evTemplate);
#endif

    fi_page_t &page(PageIndex index);

    /**
     * Find an object of the specified type with the type-unique name.
     * @param name  Unique name of the object we are looking for.

     * @return  Ptr to @c fi_object_t Either:
     *          a) Existing object associated with unique @a name.
     *          b) New object with unique @a name.
     */
    fi_object_s *findObject(fi_obtype_e type, char const *name);

    void deleteObject(fi_object_s *ob);

public: /// Script-level flow/state control (@todo make private): --------------------

    void beginDoSkipMode();
    void gotoEnd();
    void pause();
    void wait(int ticksToWait = 1);
    void foundSkipHere();
    void foundSkipMarker(char const *marker);

    int inTime() const;
    void setInTime(int seconds);

    void setHandleEvents(bool yes = true);
    void setShowMenu(bool yes = true);
    void setSkip(bool allowed = true);
    void setSkipNext(bool yes = true);
    void setWaitAnim(fi_object_s *newWaitAnim);
    void setWaitText(fi_object_s *newWaitText);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_UI_INFINE_FINALEINTERPRETER_H
