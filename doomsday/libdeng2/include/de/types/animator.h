/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ANIMATOR_H
#define LIBDENG2_ANIMATOR_H

#include "../deng.h"
#include "../Vector"
#include "../Rectangle"
#include "../IClock"
#include "../Flag"
#include "../ISerializable"

#include <sstream>

namespace de
{
    /**
     * Provides a way to gradually move between target values.
     *
     * @ingroup types
     */
    class LIBDENG2_API Animator : public ISerializable
    {
    public:
        typedef ddouble ValueType;

        enum Motion {
            EASE_OUT    ///< Motion eases out, ends abruptly.
        };

        /**
         * Interface for observing the animator's value.
         */
        class IObserver {
        public:
            virtual ~IObserver() {}  
            virtual void animatorValueSet(Animator& animator, ValueType oldTarget) = 0;
        };

        /// The animator is evaluated without an available time source. @ingroup errors
        DEFINE_ERROR(ClockMissingError);

    public:
        /**
         * Initializes the animator. By default animators use the App as the time source.
         * 
         * @param initialValue  Initial value for the animator.
         */
        Animator(ValueType initialValue = 0.0);

        Animator(const IClock& clock, ValueType initialValue = 0.0);
        Animator(const Animator& copyFrom);

        virtual ~Animator();        

        /**
         * The automatic conversion takes the animator's current value.
         *
         * @return Current value of the animator.
         */
        operator dfloat () const {
            return dfloat(now());
        }

        /**
         * The automatic conversion takes the animator's current value.
         *
         * @return Current value of the animator.
         */
        operator ddouble () const {
            return now();
        }

        operator String () const {
            std::ostringstream os;
            os << now();
            return os.str();
        }

        void setClock(const IClock& clock);

        /**
         * Begins a new animation towards a target value.
         *
         * @param targetValue Target value for the animation.
         * @param transition  Time it takes for the transition to the target.
         */
        void set(ValueType targetValue, const Time::Delta& transition = 0.0);

        /**
         * Returns the current value of the animator.
         */
        ValueType now() const;

        /**
         * Returns the target value of the animator.
         */
        ValueType target() const;

        /// Inverses the animator.
        Animator operator - () const;

        /// Multiplies the animator by a scalar.
        Animator operator * (ddouble scalar) const;

        /// Divides the animator by a scalar.
        Animator operator / (ddouble scalar) const;

        /// Apply an offset to the value.
        Animator operator + (const ValueType& offset) const;

        /// Apply an offset to the value.
        Animator operator - (const ValueType& offset) const;

        Animator& operator += (const ValueType& offset);

        Animator& operator -= (const ValueType& offset);

        /// Compares the current value.
        bool operator < (const ValueType& offset) const;

        /// Compares the current value.
        bool operator > (const ValueType& offset) const;

        /// Compares the current value.
        bool operator <= (const ValueType& offset) const {
            return !(*this > offset);
        }

        /// Compares the current value.
        bool operator >= (const ValueType& offset) const {
            return !(*this < offset);
        }

        void setObserver(IObserver* observer) { observer_ = observer; }

        IObserver* observer() const { return observer_; }
        
        // Doesn't actually implement ISerializable.
        void operator >> (Writer& /*to*/) const { assert(false); }
        void operator << (Reader& /*from*/) { assert(false); }

    private:
        const IClock* clock_;
        Motion motion_;
        mutable ValueType start_;
        Time startTime_;
        ValueType transition_;
        Time::Delta transitionTime_;
        IObserver* observer_;

        DEFINE_FINAL_FLAG(ANIMATING, 0, Status);
        mutable Status status_;
    };

    LIBDENG2_API std::ostream& operator << (std::ostream& os, const Animator& anim);

    /**
     * 2D vector animator.
     *
     * @ingroup types
     */
    class LIBDENG2_API AnimatorVector2 : public Vector2<Animator>
    {
    public:
        AnimatorVector2() {}

        AnimatorVector2(const Animator& a, const Animator& b)
            : Vector2<Animator>(a, b) {}

        AnimatorVector2(const AnimatorVector2& copyFrom) 
            : Vector2<Animator>(copyFrom.x, copyFrom.y) {}

        AnimatorVector2(const Vector2<Animator>& copyFrom) 
            : Vector2<Animator>(copyFrom.x, copyFrom.y) {}

        AnimatorVector2(const IClock& clock, Animator::ValueType initialX = 0.0, 
            Animator::ValueType initialY = 0.0) : Vector2<Animator>(initialX, initialY) {
            x.setClock(clock);
            y.setClock(clock);
        }

        void set(const Vector2f& targetValue, const Time::Delta& transition = 0) {
            x.set(targetValue.x, transition);
            y.set(targetValue.y, transition);
        }

        Vector2f now() const { 
            return Vector2f(dfloat(x.now()), dfloat(y.now()));
        }

        Vector2f target() const {
            return Vector2f(dfloat(x.target()), dfloat(y.target()));
        }

        AnimatorVector2 operator + (const Vector2<Animator::ValueType>& offset) const;

        AnimatorVector2 operator - (const Vector2<Animator::ValueType>& offset) const;

        void setObserver(Animator::IObserver* observer);
    };

    /**
     * 3D vector animator.
     *
     * @ingroup types
     */
    class LIBDENG2_API AnimatorVector3 : public AnimatorVector2
    {
    public:
        AnimatorVector3() : AnimatorVector2() {}

        AnimatorVector3(const IClock& clock, Animator::ValueType initialX = 0.0, 
            Animator::ValueType initialY = 0.0, Animator::ValueType initialZ = 0.0) 
                : AnimatorVector2(clock, initialX, initialY) {
            z.setClock(clock);
            z.set(initialZ);
        }

        void set(const Vector3f& v, const Time::Delta& transition = 0.0) {
            AnimatorVector2::set(v, transition);
            z.set(v.z, transition);
        }

        Vector3f now() const {
            return Vector3f(dfloat(x.now()), dfloat(y.now()), dfloat(z.now()));
        }

        Vector3f target() const {
            return Vector3f(dfloat(x.target()), dfloat(y.target()), dfloat(z.target()));
        }
        
    public:
        Animator z;
    };
    
    /**
     * 4D vector animator.
     *
     * @ingroup types
     */
    class LIBDENG2_API AnimatorVector4 : public AnimatorVector3
    {
    public:
        AnimatorVector4() : AnimatorVector3() {}

        AnimatorVector4(const IClock& clock, Animator::ValueType initialX = 0.0, 
            Animator::ValueType initialY = 0.0, Animator::ValueType initialZ = 0.0, 
            Animator::ValueType initialW = 0.0) : AnimatorVector3(clock, initialX, initialY, initialZ) {
            w.setClock(clock);
            w.set(initialW);
        }

        void set(const Vector4f& v, const Time::Delta& transition = 0.0) {
            AnimatorVector3::set(v, transition);
            w.set(v.w, transition);
        }

        Vector4f now() const {
            return Vector4f(dfloat(x.now()), dfloat(y.now()), 
                dfloat(z.now()), dfloat(w.now()));
        }

        Vector4f target() const {
            return Vector4f(dfloat(x.target()), dfloat(y.target()), 
                dfloat(z.target()), dfloat(w.target()));
        }
        
    public:
        Animator w;
    };

    /**
     * Rectangle animator.
     *
     * @ingroup types
     */ 
    class LIBDENG2_API AnimatorRectangle : public Rectangle<AnimatorVector2>
    {
    public:
        AnimatorRectangle(const IClock& clock, const Vector2f& tl = Vector2f(), 
            const Vector2f& br = Vector2f())  : Rectangle<AnimatorVector2>() {
            topLeft.x.setClock(clock);
            topLeft.y.setClock(clock);
            bottomRight.x.setClock(clock);
            bottomRight.y.setClock(clock);

            topLeft.x.set(tl.x);
            topLeft.y.set(tl.y);
            bottomRight.x.set(br.x);
            bottomRight.y.set(br.y);
        }

        Rectanglef now() const { 
            return Rectanglef(
                Vector2f(dfloat(topLeft.x.now()), dfloat(topLeft.y.now())),
                Vector2f(dfloat(bottomRight.x.now()), dfloat(bottomRight.y.now())));
        }
    };
}

#endif /* LIBDENG2_ANIMATOR_H */
