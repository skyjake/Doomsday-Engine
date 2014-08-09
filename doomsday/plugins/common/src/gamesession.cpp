/** @file gamesession.cpp  Logical game session and saved session marshalling.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gamesession.h"

#include <de/App>
#include <de/ArrayValue>
#include <de/NumberValue>
#include <de/RecordValue>
#include <de/Time>
#include <de/ZipArchive>
#include <de/game/SavedSession>
#include <doomsday/defs/episode.h>
#include "d_netsv.h"
#include "g_common.h"
#include "hu_menu.h"
#include "hu_inventory.h"
#include "mapstatewriter.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_savedef.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "p_sound.h"
#include "p_tick.h"
#if __JDOOM__
#  include "doomv9mapstatereader.h"
#endif
#if __JHERETIC__
#  include "hereticv13mapstatereader.h"
#endif

using namespace de;
using de::game::SavedSession;
using de::game::SessionMetadata;

namespace common {

namespace internal
{
    static String composeSaveInfo(SessionMetadata const &metadata)
    {
        String info;
        QTextStream os(&info);
        os.setCodec("UTF-8");

        // Write header and misc info.
        Time now;
        os <<   "# Doomsday Engine saved game session package.\n#"
           << "\n# Generator: GameSession (libcommon)"
           << "\n# Generation Date: " + now.asDateTime().toString(Qt::SystemLocaleShortDate);

        // Write metadata.
        os << "\n\n" + metadata.asTextWithInfoSyntax() + "\n";

        return info;
    }

    static Block serializeCurrentMapState(bool excludePlayers = false)
    {
        Block data;
        SV_OpenFileForWrite(data);
        writer_s *writer = SV_NewWriter();
        MapStateWriter().write(writer, excludePlayers);
        Writer_Delete(writer);
        SV_CloseFile();
        return data;
    }
}

using namespace internal;

static GameSession *singleton;

static String const internalSavePath = "/home/cache/internal.save";

DENG2_PIMPL(GameSession), public SavedSession::IMapStateReaderFactory
{
    GameRuleset rules; ///< current ruleset
    bool inProgress;   ///< @c true= session is in progress / internal.save exists.

    Instance(Public *i) : Base(i), inProgress(false)
    {
        DENG2_ASSERT(singleton == 0);
        singleton = thisPublic;
    }

    inline String userSavePath(String const &fileName) {
        return Session::savePath() / fileName + ".save";
    }

    void cleanupInternalSave()
    {
        // Ensure the internal save folder exists.
        App::fileSystem().makeFolder(internalSavePath.fileNamePath());

        // Ensure that any pre-existing internal save is destroyed.
        // This may happen if the game was not shutdown properly in the event of a crash.
        /// @todo It may be possible to recover this session if it was written successfully
        /// before the fatal error occurred.
        Session::removeSaved(internalSavePath);
    }

    /**
     * Returns SessionMetadata for the game configuration in progress.
     */
    SessionMetadata metadata()
    {
        DENG2_ASSERT(inProgress);

        SessionMetadata meta;

        meta.set("gameIdentityKey", Session::gameId());
        meta.set("episode",         String::number(::gameEpisode + 1));
        meta.set("userDescription", "(Unsaved)");
        meta.set("mapUri",          ::gameMapUri.compose());
        meta.set("mapTime",         ::mapTime);

        meta.add("gameRules",       self.rules().toRecord()); // Takes ownership.

        ArrayValue *array = new ArrayValue;
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            bool playerIsPresent = CPP_BOOL(players[i].plr->inGame);
            *array << NumberValue(playerIsPresent, NumberValue::Boolean);
        }
        meta.set("players", array); // Takes ownership.

        meta.set("sessionId",       uint(Timer_RealMilliseconds() + (mapTime << 24)));

        return meta;
    }

    /**
     * Update/create a new SavedSession at the specified @a path from the current
     * game state.
     */
    SavedSession &updateSavedSession(String const &path, SessionMetadata const &metadata)
    {
        DENG2_ASSERT(inProgress);

        LOG_AS("GameSession");
        LOG_RES_VERBOSE("Serializing to \"%s\"...") << path;

        // Does the .save already exist?
        SavedSession *saved = App::rootFolder().tryLocate<SavedSession>(path);
        if(saved)
        {
            DENG2_ASSERT(saved->mode().testFlag(File::Write));
            saved->replaceFile("Info") << composeSaveInfo(metadata).toUtf8();
        }
        else
        {
            // Create an empty package containing only the metadata.
            File &save = App::rootFolder().replaceFile(path);
            ZipArchive arch;
            arch.add("Info", composeSaveInfo(metadata).toUtf8());
            de::Writer(save) << arch;
            save.flush();

            // We can now reinterpret and populate the contents of the archive.
            saved = &save.reinterpret()->as<SavedSession>();
            saved->populate();
        }

        // Save the current game state to the .save package.
#if __JHEXEN__
        de::Writer(saved->replaceFile("ACScriptState")).withHeader()
                << Game_ACScriptInterpreter().serializeWorldState();
#endif

        Folder &mapsFolder = App::fileSystem().makeFolder(saved->path() / "maps");
        DENG2_ASSERT(mapsFolder.mode().testFlag(File::Write));

        mapsFolder.replaceFile(gameMapUri.path() + "State")
                << serializeCurrentMapState();

        saved->flush(); // No need to populate; FS2 Files already in sync with source data.
        saved->cacheMetadata(metadata); // Avoid immediately reopening the .save package.

        return *saved;
    }

#if __JDOOM__ || __JDOOM64__
    void applyRuleFastMonsters(dd_bool fast)
    {
        static dd_bool oldFast = false;

        // Only modify when the rule changes state.
        if(fast == oldFast) return;
        oldFast = fast;

        /// @fixme Kludge: Assumes the original values speed values haven't been modified!
        for(int i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
        {
            STATES[i].tics = fast? 1 : 2;
        }
        for(int i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
        {
            STATES[i].tics = fast? 4 : 8;
        }
        for(int i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
        {
            STATES[i].tics = fast? 1 : 2;
        }
        // Kludge end.
    }
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    void applyRuleFastMissiles(dd_bool fast)
    {
        struct missileinfo_s {
            mobjtype_t  type;
            float       speed[2];
        }
        MonsterMissileInfo[] =
        {
#if __JDOOM__ || __JDOOM64__
            { MT_BRUISERSHOT, {15, 20} },
            { MT_HEADSHOT, {10, 20} },
            { MT_TROOPSHOT, {10, 20} },
# if __JDOOM64__
            { MT_BRUISERSHOTRED, {15, 20} },
            { MT_NTROSHOT, {20, 40} },
# endif
#elif __JHERETIC__
            { MT_IMPBALL, {10, 20} },
            { MT_MUMMYFX1, {9, 18} },
            { MT_KNIGHTAXE, {9, 18} },
            { MT_REDAXE, {9, 18} },
            { MT_BEASTBALL, {12, 20} },
            { MT_WIZFX1, {18, 24} },
            { MT_SNAKEPRO_A, {14, 20} },
            { MT_SNAKEPRO_B, {14, 20} },
            { MT_HEADFX1, {13, 20} },
            { MT_HEADFX3, {10, 18} },
            { MT_MNTRFX1, {20, 26} },
            { MT_MNTRFX2, {14, 20} },
            { MT_SRCRFX1, {20, 28} },
            { MT_SOR2FX1, {20, 28} },
#endif
            { mobjtype_t(-1), {-1, -1} }
        };

        static dd_bool oldFast = false;

        // Only modify when the rule changes state.
        if(fast == oldFast) return;
        oldFast = fast;

        /// @fixme Kludge: Assumes the original values speed values haven't been modified!
        for(int i = 0; MonsterMissileInfo[i].type != -1; ++i)
        {
            MOBJINFO[MonsterMissileInfo[i].type].speed =
                MonsterMissileInfo[i].speed[fast? 1 : 0];
        }
        // Kludge end.
    }
#endif

    void applyCurrentRules()
    {
        if(rules.skill < SM_NOTHINGS)
            rules.skill = SM_NOTHINGS;
        if(rules.skill > NUM_SKILL_MODES - 1)
            rules.skill = skillmode_t(NUM_SKILL_MODES - 1);

        if(!IS_NETGAME)
        {
#if !__JHEXEN__
            rules.deathmatch      = false;
            rules.respawnMonsters = CommandLine_Check("-respawn")? true : false;

            rules.noMonsters      = CommandLine_Exists("-nomonsters")? true : false;
#endif
#if __JDOOM__ || __JHERETIC__
            // Is respawning enabled at all in nightmare skill?
            if(rules.skill == SM_NIGHTMARE)
            {
                rules.respawnMonsters = cfg.respawnMonstersNightmare;
            }
#endif
        }
        else if(IS_DEDICATED)
        {
#if !__JHEXEN__
            rules.deathmatch      = cfg.netDeathmatch;
            rules.respawnMonsters = cfg.netRespawn;

            rules.noMonsters      = cfg.netNoMonsters;
            /*rules.*/cfg.jumpEnabled = cfg.netJumping;
#else
            rules.randomClasses   = cfg.netRandomClass;
#endif
        }

        // Fast monsters?
#if __JDOOM__ || __JDOOM64__
        dd_bool fastMonsters = rules.fast;
# if __JDOOM__
        if(rules.skill == SM_NIGHTMARE)
        {
            fastMonsters = true;
        }
# endif
        applyRuleFastMonsters(fastMonsters);
#endif

        // Fast missiles?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        dd_bool fastMissiles = rules.fast;
# if !__JDOOM64__
        if(rules.skill == SM_NIGHTMARE)
        {
            fastMissiles = true;
        }
# endif
        applyRuleFastMissiles(fastMissiles);
#endif

        NetSv_UpdateGameConfigDescription();

        // Update game status cvars:
        Con_SetInteger2("game-skill", rules.skill, SVF_WRITE_OVERRIDE);
    }

    /**
     * Constructs a MapStateReader for serialized map state format interpretation.
     */
    SavedSession::MapStateReader *makeMapStateReader(
        SavedSession const &session, String const &mapUriAsText)
    {
        de::Uri const mapUri(mapUriAsText, RC_NULL);
        File const &mapStateFile = session.locateState<File const>(String("maps") / mapUri.path());
        if(!SV_OpenFileForRead(mapStateFile))
        {
            /// @throw Error The serialized map state file could not be opened for read.
            throw Error("GameSession::makeMapStateReader", "Failed to open \"" + mapStateFile.path() + "\" for read");
        }

        std::unique_ptr<SavedSession::MapStateReader> p;
        reader_s *reader = SV_NewReader();
        int const magic = Reader_ReadInt32(reader);
        if(magic == MY_SAVE_MAGIC || MY_CLIENT_SAVE_MAGIC) // Native format.
        {
            p.reset(new MapStateReader(session));
        }
#if __JDOOM__
        else if(magic == 0x1DEAD600) // DoomV9
        {
            p.reset(new DoomV9MapStateReader(session));
        }
#endif
#if __JHERETIC__
        else if(magic == 0x7D9A1200) // HereticV13
        {
            p.reset(new HereticV13MapStateReader(session));
        }
#endif
        SV_CloseFile();
        if(p.get())
        {
            return p.release();
        }

        /// @throw Error The format of the serialized map state was not recognized.
        throw Error("GameSession::makeMapStateReader", "Unrecognized map state format");
    }

    void loadSaved(String const &savePath)
    {
        ::briefDisabled = true;

        G_StopDemo();
        Hu_MenuCommand(MCMD_CLOSEFAST);
        FI_StackClear(); // Stop any running InFine scripts.

        M_ResetRandom();
        if(!IS_CLIENT)
        {
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                player_t *plr = players + i;
                if(plr->plr->inGame)
                {
                    // Force players to be initialized upon first map load.
                    plr->playerState = PST_REBORN;
#if __JHEXEN__
                    plr->worldTimer = 0;
#else
                    plr->didSecret = false;
#endif
                }
            }
        }

        inProgress = false;

        if(savePath.compareWithoutCase(internalSavePath))
        {
            // Perform necessary prep.
            cleanupInternalSave();

            // Copy the save to the internal savegame.
            Session::copySaved(internalSavePath, savePath);
        }

        /*
         * SavedSession deserialization begins.
         */
        SavedSession const &saved = App::rootFolder().locate<SavedSession>(internalSavePath);
        SessionMetadata const &metadata = saved.metadata();

        // Ensure a complete game ruleset is available.
        GameRuleset *newRules;
        try
        {
            newRules = GameRuleset::fromRecord(metadata.subrecord("gameRules"));
        }
        catch(Record::NotFoundError const &)
        {
            // The game rules are incomplete. Likely because they were missing from a savegame that
            // was converted from a vanilla format (in which most of these values are omitted).
            // Therefore we must assume the user has correctly configured the session accordingly.
            LOG_WARNING("Using current game rules as basis for loading savegame \"%s\"."
                        " (The original save format omits this information).")
                    << saved.path();

            // Use the current rules as our basis.
            newRules = GameRuleset::fromRecord(metadata.subrecord("gameRules"), &rules);
        }
        rules = *newRules;
        applyCurrentRules();
        delete newRules; newRules = 0;

#if __JHEXEN__
        // Deserialize the world ACS state.
        if(File const *state = saved.tryLocateStateFile("ACScript"))
        {
            Game_ACScriptInterpreter().readWorldState(de::Reader(*state).withHeader());
        }
#endif

        inProgress = true;

        ::gameEpisode = metadata.gets("episode").toInt() - 1;
        setMap(de::Uri(metadata.gets("mapUri"), RC_NULL));
        //::gameMapEntrance = mapEntrance; // not saved??

        reloadMap();
#if !__JHEXEN__
        ::mapTime = metadata.geti("mapTime");
#endif

        String const gameMapUriAsText = ::gameMapUri.compose();
        makeMapStateReader(saved, gameMapUriAsText)->read(gameMapUriAsText);
    }

    void setMap(de::Uri const &mapUri)
    {
        DENG2_ASSERT(inProgress);

        ::gameMapUri = mapUri;

        // Update game status cvars:
        Con_SetInteger2("map-episode", (unsigned)::gameEpisode, SVF_WRITE_OVERRIDE);
        Con_SetUri2    ("map-id",      reinterpret_cast<uri_s *>(&::gameMapUri), SVF_WRITE_OVERRIDE);
    }

    /**
     * Reload the @em current map.
     *
     * @param revisit  @c true= load progress in this map from a previous visit in the current
     * game session. If no saved progress exists then the map will be in the default state.
     */
    void reloadMap(bool revisit = false)
    {
        DENG2_ASSERT(inProgress);

        Pause_End();

        // Close open HUDs.
        for(uint i = 0; i < MAXPLAYERS; ++i)
        {
            ST_CloseAll(i, true/*fast*/);
        }

        // Delete raw images to conserve texture memory.
        DD_Executef(true, "texreset raw");

        // Are we playing a briefing? (No, if we've already visited this map).
        if(revisit)
        {
            briefDisabled = true;
        }
        char const *briefing = G_InFineBriefing(); // current map

        // Restart the map music?
        if(!briefing)
        {
#if __JHEXEN__
            /**
             * @note Kludge: Due to the way music is managed with Hexen, unless we explicitly stop
             * the current playing track the engine will not change tracks. This is due to the use
             * of the runtime-updated "currentmap" definition (the engine thinks music has not changed
             * because the current Music definition is the same).
             *
             * It only worked previously was because the waiting-for-map-load song was started prior
             * to map loading.
             *
             * @todo Rethink the Music definition stuff with regard to Hexen. Why not create definitions
             * during startup by parsing MAPINFO?
             */
            S_StopMusic();
            //S_StartMusic("chess", true); // Waiting-for-map-load song
#endif

            S_MapMusic(&gameMapUri);
            S_PauseMusic(true);
        }

        P_SetupMap(gameMapUri);

        if(revisit)
        {
            // We've been here before; deserialize the saved map state.
#if __JHEXEN__
            targetPlayerAddrs = 0; // player mobj redirection...
#endif

            SavedSession const &saved = App::rootFolder().locate<SavedSession>(internalSavePath);
            String const gameMapUriAsText = ::gameMapUri.compose();
            std::unique_ptr<SavedSession::MapStateReader> reader(
                        makeMapStateReader(saved, gameMapUriAsText));
            reader->read(gameMapUriAsText);
        }

        if(!G_StartFinale(briefing, 0, FIMODE_BEFORE, 0))
        {
            // No briefing; begin the map.
            HU_WakeWidgets(-1/* all players */);
            G_BeginMap();
        }

        Z_CheckHeap();
    }

#if __JHEXEN__
    struct playerbackup_t
    {
        player_t player;
        uint numInventoryItems[NUM_INVENTORYITEM_TYPES];
        inventoryitemtype_t readyItem;
    };

    void backupPlayersInHub(playerbackup_t playerBackup[MAXPLAYERS])
    {
        DENG2_ASSERT(playerBackup != 0);

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            playerbackup_t *pb  = playerBackup + i;
            player_t const *plr = players + i;

            std::memcpy(&pb->player, plr, sizeof(player_t));

            // Make a copy of the inventory states also.
            for(int k = 0; k < NUM_INVENTORYITEM_TYPES; ++k)
            {
                pb->numInventoryItems[k] = P_InventoryCount(i, inventoryitemtype_t(k));
            }
            pb->readyItem = P_InventoryReadyItem(i);
        }
    }

    /**
     * @param playerBackup  Player state backup.
     * @param mapEntrance   Logical entry point number used to enter the map.
     */
    void restorePlayersInHub(playerbackup_t playerBackup[MAXPLAYERS], uint mapEntrance)
    {
        DENG2_ASSERT(playerBackup != 0);

        mobj_t *targetPlayerMobj = 0;

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            playerbackup_t *pb = playerBackup + i;
            player_t *plr      = players + i;
            ddplayer_t *ddplr  = plr->plr;
            int oldKeys = 0, oldPieces = 0;
            dd_bool oldWeaponOwned[NUM_WEAPON_TYPES];

            if(!ddplr->inGame) continue;

            std::memcpy(plr, &pb->player, sizeof(player_t));

            // Reset the inventory as it will now be restored from the backup.
            P_InventoryEmpty(i);

            for(int k = 0; k < NUM_INVENTORYITEM_TYPES; ++k)
            {
                // Don't give back the wings of wrath if reborn.
                if(k == IIT_FLY && plr->playerState == PST_REBORN)
                    continue;

                for(uint m = 0; m < pb->numInventoryItems[k]; ++m)
                {
                    P_InventoryGive(i, inventoryitemtype_t(k), true);
                }
            }
            P_InventorySetReadyItem(i, pb->readyItem);

            ST_LogEmpty(i);
            plr->attacker = 0;
            plr->poisoner = 0;

            if(IS_NETGAME || rules.deathmatch)
            {
                // In a network game, force all players to be alive
                if(plr->playerState == PST_DEAD)
                {
                    plr->playerState = PST_REBORN;
                }

                if(!rules.deathmatch)
                {
                    // Cooperative net-play; retain keys and weapons.
                    oldKeys   = plr->keys;
                    oldPieces = plr->pieces;

                    for(int k = 0; k < NUM_WEAPON_TYPES; ++k)
                    {
                        oldWeaponOwned[k] = plr->weapons[k].owned;
                    }
                }
            }

            bool wasReborn = (plr->playerState == PST_REBORN);

            if(rules.deathmatch)
            {
                de::zap(plr->frags);
                ddplr->mo = 0;
                G_DeathMatchSpawnPlayer(i);
            }
            else
            {
                if(playerstart_t const *start = P_GetPlayerStart(mapEntrance, i, false))
                {
                    mapspot_t const *spot = &mapSpots[start->spot];
                    P_SpawnPlayer(i, cfg.playerClass[i], spot->origin[VX],
                                  spot->origin[VY], spot->origin[VZ], spot->angle,
                                  spot->flags, false, true);
                }
                else
                {
                    P_SpawnPlayer(i, cfg.playerClass[i], 0, 0, 0, 0,
                                  MSF_Z_FLOOR, true, true);
                }
            }

            if(wasReborn && IS_NETGAME && !rules.deathmatch)
            {
                int bestWeapon = 0;

                // Restore keys and weapons when reborn in co-op.
                plr->keys   = oldKeys;
                plr->pieces = oldPieces;

                for(int k = 0; k < NUM_WEAPON_TYPES; ++k)
                {
                    if(oldWeaponOwned[k])
                    {
                        bestWeapon = k;
                        plr->weapons[k].owned = true;
                    }
                }

                plr->ammo[AT_BLUEMANA].owned = 25; /// @todo values.ded
                plr->ammo[AT_GREENMANA].owned = 25; /// @todo values.ded

                // Bring up the best weapon.
                if(bestWeapon)
                {
                    plr->pendingWeapon = weapontype_t(bestWeapon);
                }
            }
        }

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            player_t *plr     = players + i;
            ddplayer_t *ddplr = plr->plr;

            if(!ddplr->inGame) continue;

            if(!targetPlayerMobj)
            {
                targetPlayerMobj = ddplr->mo;
            }
        }

        /// @todo Redirect anything targeting a player mobj
        /// FIXME! This only supports single player games!!
        if(targetPlayerAddrs)
        {
            for(targetplraddress_t *p = targetPlayerAddrs; p; p = p->next)
            {
                *(p->address) = targetPlayerMobj;
            }

            SV_ClearTargetPlayers();

            /*
             * When XG is available in Hexen, call this after updating target player
             * references (after a load) - ds
            // The activator mobjs must be set.
            XL_UpdateActivators();
            */
        }

        // Destroy all things touching players.
        P_TelefragMobjsTouchingPlayers();
    }
#endif // __JHEXEN__
};

GameSession::GameSession()
    : d(new Instance(this))
{}

GameSession::~GameSession()
{
    LOG_AS("~GameSession");
    d.reset();
    singleton = 0;
}

GameSession &GameSession::gameSession()
{
    DENG2_ASSERT(singleton != 0);
    return *singleton;
}

bool GameSession::hasBegun()
{
    return d->inProgress;
}

bool GameSession::loadingPossible()
{
    return !(IS_CLIENT && !Get(DD_PLAYBACK));
}

bool GameSession::savingPossible()
{
    if(IS_CLIENT || Get(DD_PLAYBACK)) return false;

    if(!hasBegun()) return false;
    if(GS_MAP != G_GameState()) return false;

    /// @todo fixme: what about splitscreen!
    player_t *player = &players[CONSOLEPLAYER];
    if(PST_DEAD == player->playerState) return false;

    return true;
}

Record *GameSession::episodeDef()
{
    if(hasBegun())
    {
        /// @todo cache this result?
        return Defs().episodes.tryFind("id", String::number(::gameEpisode + 1));
    }
    return 0;
}

Record *GameSession::mapGraphNodeDef()
{
    if(Record const *episode = episodeDef())
    {
        /// @todo cache this result?
        return defn::Episode(*episode).tryFindMapGraphNode(::gameMapUri.compose());
    }
    return 0;
}

Record *GameSession::mapInfo()
{
    if(hasBegun())
    {
        /// @todo cache this result?
        return Defs().mapInfos.tryFind("id", ::gameMapUri.compose());
    }
    return 0;
}

de::Uri GameSession::mapUriForNamedExit(String name)
{
    LOG_AS("GameSession");
    if(Record const *mgNode = mapGraphNodeDef())
    {
        // Build a lookup table mapping exit ids to exit records.
        QMap<String, Record const *> exits;
        foreach(Value *mapIt, mgNode->geta("exit").elements())
        {
            Record const &exit = mapIt->as<RecordValue>().dereference();
            String id = exit.gets("id");
            if(!id.isEmpty())
            {
                exits.insert(id, &exit);
            }
        }

        // Locate the named exit record.
        Record const *chosenExit = 0;
        if(exits.count() > 1)
        {
            QMap<String, Record const *>::const_iterator found = exits.constFind(name.toLower());
            if(found != exits.constEnd())
            {
                chosenExit = found.value();
            }
            else
            {
                LOG_SCR_WARNING("Episode '%s', map '%s' defines no Exit with ID '%s'")
                        << String::number(::gameEpisode + 1)
                        << ::gameMapUri
                        << name;
            }
        }
        else if(exits.count() == 1)
        {
            chosenExit = exits.first();
            String chosenExitId = chosenExit->gets("id");
            if(chosenExitId != name.toLower())
            {
                LOGDEV_SCR_NOTE("Exit ID:%s chosen instead of '%s'")
                        << chosenExitId << name;
            }
        }

        if(chosenExit)
        {
            return de::Uri(chosenExit->gets("targetMap"), RC_NULL);
        }
    }
    return de::Uri();
}

GameRuleset const &GameSession::rules() const
{
    return d->rules;
}

void GameSession::applyNewRules(GameRuleset const &newRules)
{
    LOG_AS("GameSession");

    d->rules = newRules;
    if(hasBegun())
    {
        d->applyCurrentRules();
        LOGDEV_WARNING("Applied new rules while in progress!");
    } // Otherwise deferred
}

bool GameSession::progressRestoredOnReload() const
{
    if(d->rules.deathmatch) return false; // Never.
#if __JHEXEN__
    return true; // Cannot be disabled.
#else
    return cfg.loadLastSaveOnReborn;
#endif
}

void GameSession::end()
{
    if(!hasBegun()) return;

#if __JHEXEN__
    Game_ACScriptInterpreter().reset();
#endif
    Session::removeSaved(internalSavePath);

    d->inProgress = false;
    LOG_MSG("Game ended");
}

void GameSession::endAndBeginTitle()
{
    end();

    if(char const *script = G_InFine("title"))
    {
        G_StartFinale(script, FF_LOCAL, FIMODE_NORMAL, "title");
        return;
    }
    /// @throw Error A title script must always be defined.
    throw Error("GameSession::endAndBeginTitle", "An InFine 'title' script must be defined");
}

void GameSession::begin(de::Uri const &mapUri, uint mapEntrance, GameRuleset const &newRules)
{
    if(hasBegun())
    {
        /// @throw InProgressError Cannot begin a new session before the current session has ended.
        throw InProgressError("GameSession::begin", "The game session has already begun");
    }

    LOG_MSG("Game begins...");

    // Check that the map truly exists.
    if(!P_MapExists(mapUri.compose().toUtf8().constData()))
    {
        throw Error("GameSession::begin", "Map \"" + mapUri.asText() + "\" does not exist");
    }

    // Perform necessary prep.
    d->cleanupInternalSave();

    G_StopDemo();

    // Close the menu if open.
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // If there are any InFine scripts running, they must be stopped.
    FI_StackClear();

    // Ignore a game action possibly set by script stop hooks; this is a completely new session.
    G_SetGameAction(GA_NONE);

    if(!IS_CLIENT)
    {
        for(uint i = 0; i < MAXPLAYERS; ++i)
        {
            player_t *plr = players + i;
            if(plr->plr->inGame)
            {
                // Force players to be initialized upon first map load.
                plr->playerState = PST_REBORN;
#if __JHEXEN__
                plr->worldTimer  = 0;
#else
                plr->didSecret   = false;
#endif
            }
        }
    }

    M_ResetRandom();

    d->rules = newRules;
    d->applyCurrentRules();
    d->inProgress = true;

    // Determine the episode for the map (should come from MapInfo).
    ::gameEpisode = G_EpisodeNumberFor(mapUri);
    d->setMap(mapUri);
    ::gameMapEntrance = mapEntrance;

    d->reloadMap();

    // Create the internal .save session package.
    d->updateSavedSession(internalSavePath, d->metadata());
}

void GameSession::reloadMap()
{
    if(!hasBegun())
    {
        /// @throw InProgressError Cannot load a map unless the game is in progress.
        throw InProgressError("GameSession::reloadMap", "No game session is in progress");
    }

    if(progressRestoredOnReload())
    {
        try
        {
            d->loadSaved(internalSavePath);
            return;
        }
        catch(Error const &er)
        {
            LOG_AS("GameSession");
            LOG_WARNING("Error loading current map state:\n") << er.asText();
        }
        // If we ever reach here then there is an insurmountable problem...
        endAndBeginTitle();
    }
    else
    {
        // Restart the session entirely.
        briefDisabled = true; // We won't brief again.
        end();
        begin(::gameMapUri, ::gameMapEntrance, d->rules);
    }
}

void GameSession::leaveMap()
{
    if(!hasBegun())
    {
        /// @throw InProgressError Cannot leave a map unless the game is in progress.
        throw InProgressError("GameSession::leaveMap", "No game session is in progress");
    }

    // If there are any InFine scripts running, they must be stopped.
    FI_StackClear();

    // Check that the map truly exists.
    if(!P_MapExists(::nextMapUri.compose().toUtf8().constData()))
    {
        ::nextMapUri = de::Uri(episodeDef()->gets("startMap"), RC_NULL); // Should exist always?
    }

#if __JHEXEN__
    // Take a copy of the player objects (they will be cleared in the process
    // of calling @ref P_SetupMap() and we need to restore them after).
    Instance::playerbackup_t playerBackup[MAXPLAYERS];
    d->backupPlayersInHub(playerBackup);

    // Disable class randomization (all players must spawn as their existing class).
    byte oldRandomClassesRule = d->rules.randomClasses;
    d->rules.randomClasses = false;
#endif

    // Are we saving progress?
    SavedSession *saved = 0;
    if(!d->rules.deathmatch) // Never save in deathmatch.
    {
        saved = &App::rootFolder().locate<SavedSession>(internalSavePath);
        Folder &mapsFolder = saved->locate<Folder>("maps");

        DENG2_ASSERT(saved->mode().testFlag(File::Write));
        DENG2_ASSERT(mapsFolder.mode().testFlag(File::Write));

        // Are we entering a new hub?
#if __JHEXEN__
        if(mapInfo()->geti("hub") != Defs().mapInfos.find("id", ::nextMapUri.compose()).geti("hub"))
#endif
        {
            // Clear all saved map states in the old hub.
            Folder::Contents contents = mapsFolder.contents();
            DENG2_FOR_EACH_CONST(Folder::Contents, i, contents)
            {
                mapsFolder.removeFile(i->first);
            }
        }
#if __JHEXEN__
        else
        {
            File &outFile = mapsFolder.replaceFile(gameMapUri.path() + "State");
            outFile << serializeCurrentMapState(true /*exclude players*/);
            // We'll flush whole package soon.
        }
#endif
        // Ensure changes are written to disk right away (otherwise would stay
        // in memory only).
        saved->flush();
    }

#if __JHEXEN__
    /// @todo Necessary?
    if(!IS_CLIENT)
    {
        // Force players to be initialized upon first map load.
        for(uint i = 0; i < MAXPLAYERS; ++i)
        {
            player_t *plr = players + i;
            if(plr->plr->inGame)
            {
                plr->playerState = PST_REBORN;
                plr->worldTimer = 0;
            }
        }
    }
    //<- todo end.

    // In Hexen the RNG is re-seeded each time the map changes.
    M_ResetRandom();
#endif

    // Change the current map.
    d->setMap(nextMapUri);
    ::gameMapEntrance = ::nextMapEntrance;

    // Are we revisiting a previous map?
    bool const revisit = saved && saved->hasState(String("maps") / gameMapUri.path());

    d->reloadMap(revisit);

    // On exit logic:
#if __JHEXEN__
    if(!revisit)
    {
        // First visit; destroy all freshly spawned players (??).
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            ddplayer_t *ddplr = players[i].plr;
            if(ddplr->inGame)
            {
                P_MobjRemove(ddplr->mo, true);
            }
        }
    }

    d->restorePlayersInHub(playerBackup, nextMapEntrance);

    // Restore the random class rule.
    d->rules.randomClasses = oldRandomClassesRule;

    // Launch waiting scripts.
    Game_ACScriptInterpreter().runDeferredTasks(gameMapUri);
#endif

    if(saved)
    {
        DENG2_ASSERT(saved->mode().testFlag(File::Write));

        SessionMetadata metadata = d->metadata();

        /// @todo Use the existing sessionId?
        //metadata.set("sessionId", saved->metadata().geti("sessionId"));
        saved->replaceFile("Info") << composeSaveInfo(metadata).toUtf8();

#if __JHEXEN__
        // Save the world-state of the ACScript interpreter.
        de::Writer(saved->replaceFile("ACScriptState")).withHeader()
                << Game_ACScriptInterpreter().serializeWorldState();
#endif

        // Save the state of the current map.
        Folder &mapsFolder = saved->locate<Folder>("maps");
        DENG2_ASSERT(mapsFolder.mode().testFlag(File::Write));

        File &outFile = mapsFolder.replaceFile(gameMapUri.path() + "State");
        outFile << serializeCurrentMapState();

        // Write all changes to the package.
        saved->flush();

        saved->cacheMetadata(metadata); // Avoid immediately reopening the .save package.
    }
}

String GameSession::userDescription()
{
    if(!hasBegun()) return "";
    return App::rootFolder().locate<SavedSession>(internalSavePath).metadata().gets("userDescription", "");
}

static String chooseSaveDescription(String const &savePath, String const &userDescription)
{
    // Use the user description given.
    if(!userDescription.isEmpty())
    {
        return userDescription;
    }
    // We'll generate a suitable description automatically.
    return G_DefaultSavedSessionUserDescription(savePath.fileNameWithoutExtension());
}

void GameSession::save(String const &saveName, String const &userDescription)
{
    if(!hasBegun())
    {
        /// @throw InProgressError Cannot save unless the game is in progress.
        throw InProgressError("GameSession::save", "No game session is in progress");
    }

    String const savePath = d->userSavePath(saveName);
    LOG_MSG("Saving game to \"%s\"...") << savePath;

    try
    {
        // Compose the session metadata.
        SessionMetadata metadata = d->metadata();
        metadata.set("userDescription", chooseSaveDescription(savePath, userDescription));

        // Update the existing internal .save package.
        d->updateSavedSession(internalSavePath, metadata);

        // In networked games the server tells the clients to save also.
        NetSv_SaveGame(metadata.geti("sessionId"));

        // Copy the internal saved session to the destination slot.
        Session::copySaved(savePath, internalSavePath);

        P_SetMessage(&players[CONSOLEPLAYER], 0, TXT_GAMESAVED);

        // Notify the engine that the game was saved.
        /// @todo After the engine has the primary responsibility of saving the game,
        /// this notification is unnecessary.
        Plug_Notify(DD_NOTIFY_GAME_SAVED, NULL);
    }
    catch(Error const &er)
    {
        LOG_RES_WARNING("Error saving game session to '%s':\n")
                << savePath << er.asText();
    }
}

/// @todo Use busy mode here.
void GameSession::load(String const &saveName)
{
    String const savePath = d->userSavePath(saveName);
    LOG_MSG("Loading game from \"%s\"...") << savePath;
    d->loadSaved(savePath);
    P_SetMessage(&players[CONSOLEPLAYER], 0, "Game loaded");
}

void GameSession::copySaved(String const &destName, String const &sourceName)
{
    Session::copySaved(d->userSavePath(destName), d->userSavePath(sourceName));
    LOG_MSG("Copied savegame \"%s\" to \"%s\"") << sourceName << destName;
}

void GameSession::removeSaved(String const &saveName)
{
    Session::removeSaved(d->userSavePath(saveName));
}

String GameSession::savedUserDescription(String const &saveName)
{
    String const savePath = d->userSavePath(saveName);
    if(SavedSession const *saved = App::rootFolder().tryLocate<SavedSession>(savePath))
    {
        return saved->metadata().gets("userDescription", "");
    }
    return ""; // Not found.
}

namespace {
int gsvRuleSkill;
int gsvEpisode;
uri_s *gsvMap;
#if __JHEXEN__
int gsvHub;
#endif
}

void GameSession::consoleRegister() //static
{
#define READONLYCVAR  (CVF_READ_ONLY | CVF_NO_MAX | CVF_NO_MIN | CVF_NO_ARCHIVE)

    C_VAR_INT("game-skill",  &gsvRuleSkill, READONLYCVAR, 0, 0);
    C_VAR_INT("map-episode", &gsvEpisode,   READONLYCVAR, 0, 0);
#if __JHEXEN__
    C_VAR_INT("map-hub",     &gsvHub,       READONLYCVAR, 0, 0);
#endif
    C_VAR_URIPTR("map-id",   &gsvMap,       READONLYCVAR, 0, 0);

#undef READONLYCVAR
}

} // namespace common
