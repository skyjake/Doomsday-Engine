#ifndef DD_OBJECT_H
#define DD_OBJECT_H

#include "dd_share.h"
#include <cstring>

#ifndef __cplusplus
#   error "C++ compilation required!"
#endif

class mobj_s 
{
public:
    thinker_t       thinker;            /* thinker node */ 
    float           pos[3];
    nodeindex_t     lineRoot; /* lines to which this is linked */ 
    struct mobj_s*  sNext, **sPrev; /* links in sector (if needed) */ 

    struct subsector_s* subsector; /* subsector in which this resides */ 
    float           mom[3]; 
    angle_t         angle; 
    spritenum_t     sprite; /* used to find patch_t and flip value */ 
    int             frame; 
    float           radius; 
    float           height; 
    int             ddFlags; /* Doomsday mobj flags (DDMF_*) */ 
    float           floorClip; /* value to use for floor clipping */ 
    int             valid; /* if == valid, already checked */ 
    int             type; /* mobj type */ 
    struct state_s* state; 
    int             tics; /* state tic counter */ 
    float           floorZ; /* highest contacted floor */ 
    float           ceilingZ; /* lowest contacted ceiling */ 
    struct mobj_s*  onMobj; /* the mobj this one is on top of. */ 
    boolean         wallHit; /* the mobj is hitting a wall. */ 
    struct ddplayer_s* dPlayer; /* NULL if not a player mobj. */ 
    float           srvo[3]; /* short-range visual offset (xyz) */ 
    short           visAngle; /* visual angle ("angle-servo") */ 
    int             selector; /* multipurpose info */ 
    int             validCount; /* used in iterating */ 
    int             addFrameCount; 
    unsigned int    lumIdx; /* index+1 of the lumobj/bias source, or 0 */ 
    byte            haloFactors[DDMAXPLAYERS]; /* strengths of halo */ 
    byte            translucency; /* default = 0 = opaque */ 
    short           visTarget; /* -1 = mobj is becoming less visible, */ 
                               /* 0 = no change, 2= mobj is becoming more visible */ 
    int             reactionTime; /* if not zero, freeze controls */ 
    int             tmap, tclass;
    
    mobj_s() :
        lineRoot(0),
        sNext(0),
        sPrev(0),
        subsector(0),
        angle(0),
        sprite(0),
        frame(0),
        radius(0),
        height(0),
        ddFlags(0),
        floorClip(0),
        valid(0),
        type(0),
        state(0),
        tics(0),
        floorZ(0),
        ceilingZ(0),
        onMobj(0),
        wallHit(0),
        dPlayer(0),
        visAngle(0),
        selector(0),
        validCount(0),
        addFrameCount(0),
        lumIdx(0),
        translucency(0),
        visTarget(0),
        reactionTime(0),
        tmap(0),
        tclass(0)
    {
        std::memset(&thinker, 0, sizeof(thinker));
        pos[0] = pos[1] = pos[2] = 0;
        mom[0] = mom[1] = mom[2] = 0;
        srvo[0] = srvo[1] = srvo[2] = 0;
        std::memset(haloFactors, 0, sizeof(haloFactors));
    }
};

#endif /* DD_OBJECT_H */
