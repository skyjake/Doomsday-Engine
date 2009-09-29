/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_MAP_H
#define LIBDENG2_MAP_H

#include "../ISerializable"
#include "../Enumerator"
#include "../Record"
#include "../String"
#include "../Id"

#include <map>

namespace de
{
    class Thinker;
    class Object;
    
    /**
     * Contains everything that makes a map work: sectors, lines, scripts, 
     * objects, etc. The game plugin is responsible for creating concrete
     * instances of Map. The game plugin can extend this with whatever 
     * information it needs.
     *
     * @ingroup world
     */
    class LIBDENG2_API Map : public ISerializable
    {
    public:
        /// Requested type casting was impossible. @ingroup errors
        DEFINE_ERROR(TypeError);
        
        /// The thinker or object that was looked for could not be found. @ingroup errors
        DEFINE_ERROR(NotFoundError);
        
        typedef std::map<Id, Thinker*> Thinkers;
        typedef std::map<Id, Object*> Objects;
                
    public:
        /**
         * Constructs an empty map.
         */
        Map();
        
        virtual ~Map();

        void clear();
        
        /**
         * Loads a map.
         *
         * @param name  Identifier of the map. The resources of the map are
         *              located based on this identifier.
         */
        virtual void load(const String& name);

        const String& name() const { return _name; }

        const Record& info() const { return _info; }

        Record& info() { return _info; }

        /**
         * Determines whether the map is void. A map is void when no map data 
         * has been loaded.
         */
        bool isVoid() const;

        /**
         * Returns a new unique thinker id.
         */
        Id findUniqueThinkerId();

        /**
         * Creates a new object in the map.
         *
         * @return  The new object. Map keeps ownership.
         */
        Object& newObject();

        /**
         * Adds a thinker to the map. The thinker will be assigned a new unique
         * id from the map's Enumerator.
         *
         * @param thinker  Thinker to add. Map takes ownership.
         */
        Thinker& add(Thinker* thinker);

        template <typename Type>
        Type& addAs(Thinker* thinker) {
            return *static_cast<Type*>(&add(thinker));
        }
        
        /**
         * Removes a thinker from the map. 
         *
         * @return  Thinker. Ownership given to caller.
         */
        Thinker* remove(Thinker& thinker);

        /**
         * Returns a map of thinkers, excluding objects.
         */
        const Thinkers& thinkers() const { return _thinkers; }
        
        /**
         * Returns a map of objects.
         */
        const Objects& objects() const { return _objects; }

        /**
         * Returns a thinker with the specified id, or @c NULL if it doesn't exist.
         * Does not return objects.
         */
        Thinker* thinker(const Id& id) const;
        
        /**
         * Returns an object with the specified id, or @c NULL if it doesn't exist.
         */
        Object* object(const Id& id) const;

        /**
         * Finds any thinker (regular or object) with the specified id.
         *
         * @param id  Thinker/object id.
         *
         * @return  Thinker casted to @a Type.
         */
        template <typename Type>
        Type& anyThinker(const Id& id) const {
            Type* o = dynamic_cast<Type*>(object(id));
            if(o)
            {
                return *o;
            }
            Type* t = dynamic_cast<Type*>(thinker(id));
            if(t)
            {
                return *t;
            }
            /// @throw TypeError  Thinker's type was different than expected.
            throw TypeError("Map::anyThinker", "Thinker not found, or has unexpected type");
        }

        /**
         * Performs thinking for all thinkers, including objects.
         *
         * @param elapsed  Amount of time elapsed since previous thinking.
         */
        void think(const Time::Delta& elapsed);

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    protected:
        void addThinkerOrObject(Thinker* thinker);
        
    private:
        /// Name of the map.
        String _name;
        
        /// Map-specific information. Lost when the map changes.
        Record _info;
        
        /// Generates ids for thinkers (objects, too).
        Enumerator _thinkerEnum;
        
        /// All thinkers of the map (except objects).
        Thinkers _thinkers;

        /// All objects of the map.
        Objects _objects;
    };
}

#endif /* LIBDENG2_MAP_H */
