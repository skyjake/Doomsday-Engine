#ifndef __DGL_PARTICLES_H__
#define __DGL_PARTICLES_H__

extern int		r_use_particles, r_max_particles;
extern float	r_particle_spawn_rate;
extern int		rend_particle_nearlimit;
extern float	rend_particle_diffuse;

void PG_InitTextures(void);
void PG_ShutdownTextures(void);
void PG_InitForLevel(void);
void PG_InitForNewFrame(void);
void PG_SectorIsVisible(sector_t *sector);
void PG_Render(void);

#endif