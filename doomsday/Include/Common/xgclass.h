#ifndef __XG_LINE_CLASSES_H__
#define __XG_LINE_CLASSES_H__

enum // Line type classes.
{
	LTC_NONE,			// No action.
	LTC_CHAIN_SEQUENCE,
	LTC_PLANE_MOVE,
	LTC_BUILD_STAIRS,
	LTC_DAMAGE,
	LTC_POWER,
	LTC_LINE_TYPE,
	LTC_SECTOR_TYPE,
	LTC_SECTOR_LIGHT,
	LTC_ACTIVATE,
	LTC_KEY,
	LTC_MUSIC,			// Change the music to play.
	LTC_LINE_COUNT,		// Line activation count delta.
	LTC_END_LEVEL,
	LTC_DISABLE_IF_ACTIVE,
	LTC_ENABLE_IF_ACTIVE,
	LTC_EXPLODE,		// Explodes the activator.
	LTC_PLANE_TEXTURE,
	LTC_WALL_TEXTURE,
	LTC_COMMAND,
	LTC_SOUND,			// Play a sector sound.
	LTC_MIMIC_SECTOR,
	NUM_LINE_CLASSES
};

#endif
