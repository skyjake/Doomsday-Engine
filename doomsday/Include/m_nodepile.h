//===========================================================================
// M_NODEPILE.H
//===========================================================================
#ifndef __DOOMSDAY_NODEPILE_H__
#define __DOOMSDAY_NODEPILE_H__

#define NP_ROOT_NODE	((void*) -1 )

struct linknode_s;	// Defined in dd_share.h.

typedef struct nodepile_s
{
	int	count;
	int pos;
	struct linknode_s *nodes;
} nodepile_t;

void NP_Init(nodepile_t *pile, int initial);
nodeindex_t NP_New(nodepile_t *pile, void *ptr);
void NP_Link(nodepile_t *pile, nodeindex_t node, nodeindex_t root);
void NP_Unlink(nodepile_t *pile, nodeindex_t node);
/* void NP_Dismiss(nodepile_t *pile, nodeindex_t node); */

#define NP_Dismiss(pile, node) (pile.nodes[node].ptr = 0)

#endif 