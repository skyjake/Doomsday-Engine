#ifndef DOOMSDAY_API_RESOURCECLASS_H
#define DOOMSDAY_API_RESOURCECLASS_H

/**
 * Resource Class Identifier.
 *
 * @ingroup base
 *
 * @todo Refactor away. These identifiers are no longer needed.
 */
typedef enum resourceclassid_e {
    RC_NULL = -2,           ///< Not a real class.
    RC_UNKNOWN = -1,        ///< Attempt to guess the class through evaluation of the path.
    RESOURCECLASS_FIRST = 0,
    RC_PACKAGE = RESOURCECLASS_FIRST,
    RC_DEFINITION,
    RC_GRAPHIC,
    RC_MODEL,
    RC_SOUND,
    RC_MUSIC,
    RC_FONT,
    RESOURCECLASS_COUNT
} resourceclassid_t;

#define VALID_RESOURCECLASSID(n)   ((n) >= RESOURCECLASS_FIRST && (n) < RESOURCECLASS_COUNT)

#endif // DOOMSDAY_API_RESOURCECLASS_H
