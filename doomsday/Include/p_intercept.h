//===========================================================================
// P_INTERCEPT.H
//===========================================================================
#ifndef __DOOMSDAY_PLAY_INTERCEPT_H__
#define __DOOMSDAY_PLAY_INTERCEPT_H__

void			P_ClearIntercepts(void);
intercept_t *	P_AddIntercept(fixed_t frac, boolean isaline, void *ptr);
boolean			P_TraverseIntercepts(traverser_t func, fixed_t maxfrac);
boolean			P_SightTraverseIntercepts(divline_t *strace, boolean (*func)(intercept_t*));

#endif 