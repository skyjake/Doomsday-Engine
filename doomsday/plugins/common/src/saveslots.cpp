/** @file saveslots.cpp  Map of logical game save slots.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "saveslots.h"

#include "p_saveio.h"
#include <de/NativePath>
#include <de/String>
#include <vector>

#define MAX_HUB_MAPS 99

static int cvarLastSlot  = -1; ///< @c -1= Not yet loaded/saved in this game session.
static int cvarQuickSlot = -1; ///< @c -1= Not yet chosen/determined.

DENG2_PIMPL_NOREF(SaveSlots::Slot)
{
    int index;
    SaveInfo *info;

    Instance(int index) : index(index), info(0) {}
    ~Instance() { delete info; }
};

SaveSlots::Slot::Slot(int index) : d(new Instance(index))
{}

int SaveSlots::Slot::index() const
{
    return d->index;
}

bool SaveSlots::Slot::isUsed() const
{
    if(SV_ExistingFile(savePath()))
    {
        return saveInfo().isLoadable();
    }
    return false;
}

bool SaveSlots::Slot::hasSaveInfo() const
{
    return d->info != 0;
}

void SaveSlots::Slot::clearSaveInfo()
{
    delete d->info; d->info = 0;
}

void SaveSlots::Slot::replaceSaveInfo(SaveInfo *newInfo)
{
    clearSaveInfo();
    d->info = newInfo;
}

void SaveSlots::Slot::addMissingSaveInfo()
{
    if(d->info) return;
    d->info = new SaveInfo;
    d->info->updateFromFile(savePath());
}

SaveInfo &SaveSlots::Slot::saveInfo(bool canCreate) const
{
    if(!d->info && canCreate)
    {
        const_cast<SaveSlot *>(this)->addMissingSaveInfo();
    }
    if(d->info)
    {
        return *d->info;
    }
    /// @throw MissingInfoError Attempted with no SaveInfo present.
    throw MissingInfoError("SaveSlots::Slot::saveInfo", "No SaveInfo exists");
}

de::Path SaveSlots::Slot::mapSavePath(uint map) const
{
    // Do we have a valid path?
    /// @todo Do not do alter the file system until necessary.
    if(!F_MakePath(de::NativePath(SV_SavePath()).expand().toUtf8().constData()))
    {
        return "";
    }

    // Compose the full game-save path and filename.
    return SV_SavePath()
                / de::String(SAVEGAMENAME "%1%2." SAVEGAMEEXTENSION)
                        .arg(d->index)
                        .arg(int(map + 1), 2, 10, QChar('0'));
}

de::Path SaveSlots::Slot::savePath() const
{
    // Do we have a valid path?
    /// @todo Do not do alter the file system until necessary.
    if(!F_MakePath(de::NativePath(SV_SavePath()).expand().toUtf8().constData()))
    {
        return "";
    }

    // Compose the full game-save path and filename.
    return SV_SavePath()
                / de::String(SAVEGAMENAME "%1." SAVEGAMEEXTENSION)
                        .arg(d->index);
}

DENG2_PIMPL(SaveSlots)
{
    int slotCount;
    typedef std::vector<Slot *> Slots;
    Slots sslots;
    Slot autoSlot;
#if __JHEXEN__
    Slot baseSlot;
#endif

    Instance(Public *i, int slotCount)
        : Base(i)
        , slotCount(de::max(1, slotCount))
        , autoSlot(AUTO_SLOT)
#if __JHEXEN__
        , baseSlot(BASE_SLOT)
#endif
    {
        for(int i = 0; i < slotCount; ++i)
        {
            sslots.push_back(new Slot(i));
        }
    }

    ~Instance()
    {
        DENG2_FOR_EACH(Slots, i, sslots) { delete *i; }
    }

    /// Determines whether to announce when the specified @a slotNumber is cleared.
    bool shouldAnnounceWhenClearing(int slotNumber)
    {
#ifdef DENG_DEBUG
        return true; // Always.
#endif
#if __JHEXEN__
        return (slotNumber != AUTO_SLOT && slotNumber != BASE_SLOT);
#else
        return (slotNumber != AUTO_SLOT);
#endif
    }

    /// Re-build save info by re-scanning the save paths and populating the list.
    void buildInfosIfNeeded()
    {
        // Scan the save paths and populate the list.
        /// @todo We should look at all files on the save path and not just those which match
        /// the default game-save file naming convention.
        for(int i = 0; i < (signed)sslots.size(); ++i)
        {
            sslots[i]->addMissingSaveInfo();
        }
        autoSlot.addMissingSaveInfo();
#if __JHEXEN__
        baseSlot.addMissingSaveInfo();
#endif
    }
};

SaveSlots::SaveSlots(int slotCount) : d(new Instance(this, slotCount))
{}

void SaveSlots::clearAll()
{
    DENG2_FOR_EACH(Instance::Slots, i, d->sslots)
    {
        (*i)->clearSaveInfo();
    }
    d->autoSlot.clearSaveInfo();
#if __JHEXEN__
    d->baseSlot.clearSaveInfo();
#endif

    // Reset last-used and quick-save slot tracking.
    Con_SetInteger2("game-save-last-slot", -1, SVF_WRITE_OVERRIDE);
    Con_SetInteger("game-save-quick-slot", -1);
}

void SaveSlots::updateAll()
{
    d->buildInfosIfNeeded();
}

de::String SaveSlots::slotIdentifier(int slot) const
{
    if(slot < 0) return "(invalid slot)";
    if(slot == AUTO_SLOT) return "<auto>";
#if __JHEXEN__
    if(slot == BASE_SLOT) return "<base>";
#endif
    return de::String::number(slot);
}

int SaveSlots::parseSlotIdentifier(de::String str) const
{
    // Try game-save name match.
    int slot = findSlotWithUserSaveDescription(str);
    if(slot >= 0) return slot;

    // Try keyword identifiers.
    if(!str.compareWithoutCase("last") || !str.compareWithoutCase("<last>"))
    {
        return Con_GetInteger("game-save-last-slot");
    }
    if(!str.compareWithoutCase("quick") || !str.compareWithoutCase("<quick>"))
    {
        return Con_GetInteger("game-save-quick-slot");
    }
    if(!str.compareWithoutCase("auto") || !str.compareWithoutCase("<auto>"))
    {
        return AUTO_SLOT;
    }

    // Try logical slot identifier.
    bool ok;
    slot = str.toInt(&ok);
    if(ok) return slot;

    // Unknown/not found.
    return -1;
}

int SaveSlots::findSlotWithUserSaveDescription(de::String description) const
{
    if(!description.isEmpty())
    {
        DENG2_FOR_EACH_CONST(Instance::Slots, i, d->sslots)
        {
            SaveSlot &sslot = **i;
            if(sslot.hasSaveInfo() &&
               !sslot.saveInfo().userDescription().compareWithoutCase(description))
            {
                return sslot.index();
            }
        }
    }
    return -1; // Not found.
}

int SaveSlots::slotCount() const
{
    return d->slotCount;
}

bool SaveSlots::isKnownSlot(int slot) const
{
    if(slot == AUTO_SLOT) return true;
#if __JHEXEN__
    if(slot == BASE_SLOT) return true;
#endif
    return (slot >= 0  && slot < d->slotCount);
}

bool SaveSlots::slotIsUserWritable(int slot) const
{
    if(slot == AUTO_SLOT) return false;
#if __JHEXEN__
    if(slot == BASE_SLOT) return false;
#endif
    return isKnownSlot(slot);
}

SaveSlots::Slot &SaveSlots::slot(int slotNumber) const
{
    if(!isKnownSlot(slotNumber))
    {
        /// @throw InvalidSlotError An invalid slot was specified.
        throw InvalidSlotError("SaveSlots::slot", "Invalid slot #" + de::String::number(slotNumber));
    }

    d->buildInfosIfNeeded();

    if(slotNumber == AUTO_SLOT) return d->autoSlot;
#if __JHEXEN__
    if(slotNumber == BASE_SLOT) return d->baseSlot;
#endif
    DENG2_ASSERT(slotNumber >= 0 && slotNumber < int( d->sslots.size() ));
    return *d->sslots[slotNumber];
}

void SaveSlots::clearSlot(int slotNumber)
{
    Slot &sslot = slot(slotNumber);

    sslot.addMissingSaveInfo();

    if(d->shouldAnnounceWhenClearing(slotNumber))
    {
        App_Log(DE2_RES_MSG, "Clearing save slot %s", slotIdentifier(slotNumber).toLatin1().constData());
    }

    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        SV_RemoveFile(sslot.mapSavePath(i));
    }

    SV_RemoveFile(sslot.savePath());

    sslot.saveInfo().setUserDescription("");
    sslot.saveInfo().setSessionId(0);
}

void SaveSlots::copySlot(int sourceSlotNumber, int destSlotNumber)
{
    LOG_AS("SaveSlots::copySlot");

    Slot &sourceSlot = slot(sourceSlotNumber);
    Slot &destSlot   = slot(destSlotNumber);

    // Clear all save files at destination slot.
    clearSlot(destSlotNumber);

    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        SV_CopyFile(sourceSlot.mapSavePath(i), destSlot.mapSavePath(i));
    }

    SV_CopyFile(sourceSlot.savePath(), destSlot.savePath());

    // Copy save info too.
    destSlot.replaceSaveInfo(new SaveInfo(sourceSlot.saveInfo()));
}

void SaveSlots::consoleRegister() // static
{
#if !__JHEXEN__
    C_VAR_BYTE("game-save-auto-loadonreborn",    &cfg.loadAutoSaveOnReborn,  0, 0, 1);
#endif
    C_VAR_BYTE("game-save-confirm",              &cfg.confirmQuickGameSave,  0, 0, 1);
    C_VAR_BYTE("game-save-confirm-loadonreborn", &cfg.confirmRebornLoad,     0, 0, 1);
    C_VAR_BYTE("game-save-last-loadonreborn",    &cfg.loadLastSaveOnReborn,  0, 0, 1);
    C_VAR_INT ("game-save-last-slot",            &cvarLastSlot, CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, 0, 0);
    C_VAR_INT ("game-save-quick-slot",           &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);

    // Aliases for obsolete cvars:
    C_VAR_BYTE("menu-quick-ask",                 &cfg.confirmQuickGameSave, 0, 0, 1);
}
