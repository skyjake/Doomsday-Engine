//===========================================================================
// P_THINK.H
//===========================================================================
#ifndef __DOOMSDAY_THINKER_H__
#define __DOOMSDAY_THINKER_H__

// think_t is a function pointer to a routine to handle an actor
typedef void (*think_t) ();

typedef struct thinker_s
{
	struct		thinker_s	*prev, *next;
	think_t		function;
	thid_t		id;			// Only used for mobjs (zero is not an ID).
} thinker_t;

extern thinker_t thinkercap; // both the head and tail of the thinker list

void P_RunThinkers(void);
void P_InitThinkers(void);
void P_AddThinker(thinker_t *thinker);
void P_RemoveThinker(thinker_t *thinker);
void P_SetMobjID(thid_t id, boolean state);
boolean P_IsUsedMobjID(thid_t id);
boolean P_IsMobjThinker(think_t thinker);

#endif

