/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_THINKER_H
#define LIBDENG2_THINKER_H

#include "../ISerializable"
#include "../Time"
#include "../Enumerator"
#include "../Id"
#include "../Reader"

#include <map>
#include <QFlags>

namespace de
{
    class Map;
    class Reader;
    class Record;
    
    /**
     * An independent entity that periodically executes a "thinker" function
     * to update its state and perform actions. Games will derive thinkers
     * specific to their built-in world behaviors.
     *
     * @ingroup world
     */
    class Thinker : public ISerializable
    {
    public:
        /// Unrecognized type encountered when deserializing a thinker. @ingroup errors
        DEFINE_ERROR(UnrecognizedError);
        
        /// Invalid type when deserializing a thinker. @ingroup errors
        DEFINE_ERROR(InvalidTypeError);

        enum ModeFlag
        {
            /// The thinker is in statis and will not think.
            InStasis = 0x1
        };
        Q_DECLARE_FLAGS(ModeFlags, ModeFlag);

        /// Identifier used when serializing thinkers (0-255).
        typedef duint8 SerialId;

        enum {
            THINKER = 0,
            OBJECT = 1,
            FIRST_CUSTOM_THINKER = 10
        };
        
        /**
         * Thinker constructor function. Constructs a new instance of the thinker.
         */
        typedef Thinker* (*Constructor)();
        
    public:
        /**
         * Constructs a new thinker. Note that the ID is not initialized here
         * (an enumerator will generate it, or it will be deserialized).
         *
         * @param serialId  Serialization identifier of the thinker class.
         */
        Thinker(SerialId serialId = THINKER);
        
        virtual ~Thinker();
        
        /**
         * Thinkers that are "alive" will think on every iteration of the main loop.
         */
        bool isAlive() const { return !mode.testFlag(InStasis); }
        
        /**
         * Returns the ID of the thinker.
         */
        const Id& id() const { return _id; }
        
        /**
         * Returns the serialization ID of the thinker class.
         */
        const SerialId& serialId() const { return _serialId; }
        
        /**
         * Sets the id of the thinker.
         *
         * @param id  Id.
         */
        void setId(const Id& id);
        
        /**
         * Sets the map of the thinker. Every thinker is in at most one map.
         *
         * @param map  Map.
         */
        void setMap(Map* map);

        /**
         * Returns the map of the thinker.
         */
        Map* map() const { return _map; }
        
        /**
         * Perform thinking. If there is a function called "thinker" present
         * in the private namespace, it will be automatically called.
         * 
         * @param elapsed  Time elapsed since the last think() call.
         */
        virtual void think(const Time::Delta& elapsed);
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);

    public:
        /**
         * Constructs a new instance of the thinker. Derived classes are required
         * to override this with their own constructors.
         */
        static Thinker* construct() {
            return new Thinker;
        }
        
        /**
         * Defines a new thinker type that can be (de)serialized.
         *
         * @param serializedId  Identifier of the thinker type when serialized.
         * @param constructor   Function that constructs thinkers.
         */
        static void define(SerialId serializedId, Constructor constructor);
        
        /**
         * Undefines a thinker type.
         *
         * @param serializedId  Identifier of the thinker type when serialized.
         */
        static void undefine(SerialId serializedId);

        /**
         * Constructs a new thinker by reading one from a Reader. Calls all the defined
         * thinker constructors until an instance is successfully constructed.
         *
         * @param reader  Reader.
         *
         * @return  Thinker. Caller gets ownership.
         *
         * @see define(), undefine()
         */
        static Thinker* constructFrom(Reader& reader);

    public:
        /// Mode flags.
        ModeFlags mode;

    private:
        /// Identifier of the thinker class for serialization.
        SerialId _serialId;
        
        /// Unique identifier for the thinker.
        Id _id;
        
        /// Time when the thinker was initially created.
        Time _bornAt;

        /// Optional thinker-specific namespace.
        Record* _info;
        
        /// The map where the thinker is in.
        Map* _map;
        
        typedef std::map<SerialId, Constructor> Constructors;
        static Constructors _constructors;
    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(de::Thinker::ModeFlags);

#endif /* LIBDENG2_THINKER_H */
