/** @file gamestatereader.h  Saved game state reader.
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

#ifndef LIBCOMMON_GAMESTATEREADER_H
#define LIBCOMMON_GAMESTATEREADER_H

#include "common.h"
#include "saveinfo.h"
#include <de/Error>
#include <de/Path>

/**
 * Interface for game state (savegame) readers.
 */
class IGameStateReader
{
public:
    /// An error occurred attempting to open the input file. @ingroup errors
    DENG2_ERROR(FileAccessError);

    /// Base class for read errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    virtual ~IGameStateReader() {}

    /**
     * Attempt to load (read/interpret) the saved game state.
     *
     * @param info  SaveInfo for the saved game state to be read/interpreted.
     * @param path  Path to the saved game state to be read.
     *
     * @todo @a path should be provided by SaveInfo.
     */
    virtual void read(SaveInfo &info, de::Path path) = 0;
};

/**
 * Game state recognizer function ptr.
 *
 * The job of a recognizer function is to determine whether the resource file on @a path is
 * interpretable as a potentially loadable savegame state.
 *
 * @param info  SaveInfo to attempt to read game session header into.
 * @param path  Path to the resource file to be recognized.
 */
typedef bool (*GameStateRecognizeFunc)(SaveInfo &info, de::Path path);

/// Game state reader instantiator function ptr.
typedef IGameStateReader *(*GameStateReaderMakeFunc)();

/**
 * Factory for the construction of new IGameStateReader-derived instances.
 */
class GameStateReaderFactory
{
public:
    /**
     * Register a game state reader.
     *
     * @param recognizer  Game state recognizer function.
     * @param maker       Game state reader instantiator function.
     */
    void declareReader(GameStateRecognizeFunc recognizer, GameStateReaderMakeFunc maker)
    {
        DENG_ASSERT(recognizer != 0 && maker != 0);
        ReaderInfo info;
        info.recognize = recognizer;
        info.newReader = maker;
        readers.push_back(info);
    }

    /**
     * Determines whether a IGameStateReader appropriate for the specified save game @a info
     * is available and if so, reads the game session header.
     *
     * @param saveInfo  The game session header will be written here if recognized.
     * @param path      Path to the resource file to be recognized.
     *
     * @return  @c true= the game session header was read successfully.
     *
     * @see recognizeAndMakeReader()
     */
    bool recognize(SaveInfo &saveInfo, de::Path path) const
    {
        return readGameSessionHeader(saveInfo, path) != 0;
    }

    /**
     * Determines whether a IGameStateReader appropriate for the specified save game @a info
     * is available and if so, reads the game session header and returns a new reader instance
     * for deserializing the game state.
     *
     * @param saveInfo  The game session header will be written here if recognized.
     * @param path      Path to the resource file to be recognized.
     *
     * @return  New reader instance if recognized; otherwise @c 0. Ownership given to the caller.
     *
     * @see recognize()
     */
    IGameStateReader *recognizeAndMakeReader(SaveInfo &saveInfo, de::Path path) const
    {
        if(ReaderInfo const *rdrInfo = readGameSessionHeader(saveInfo, path))
        {
            return rdrInfo->newReader();
        }
        return 0; // Unrecognized
    }

private:
    struct ReaderInfo {
        GameStateRecognizeFunc recognize;
        GameStateReaderMakeFunc newReader;
    };
    typedef std::list<ReaderInfo> ReaderInfos;
    ReaderInfos readers;

    ReaderInfo const *readGameSessionHeader(SaveInfo &info, de::Path path) const
    {
        DENG2_FOR_EACH_CONST(ReaderInfos, i, readers)
        {
            if(i->recognize(info, path))
            {
                return &*i;
            }
        }
        return 0; // Unrecognized
    }
};

/**
 * Native saved game state reader.
 *
 * @ingroup libcommon
 * @see GameStateWriter
 */
class GameStateReader : public IGameStateReader
{
public:
    GameStateReader();
    ~GameStateReader();

    static IGameStateReader *make();
    static bool recognize(SaveInfo &info, de::Path path);

    void read(SaveInfo &info, de::Path path);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_GAMESTATEREADER_H
