//===========================================================================
// SYS_MUSD.H
//===========================================================================
#ifndef __DOOMSDAY_MUSIC_DRIVER_H__
#define __DOOMSDAY_MUSIC_DRIVER_H__

typedef struct musdriver_s
{
	int			(*Init)(void);
	void		(*Shutdown)(void);
} 
musdriver_t;

// Music interface properties.
enum
{
	MUSIP_ID,			// Only for Get()ing.
	MUSIP_VOLUME
};

// Generic driver interface. All other interfaces are based on this.
typedef struct musinterface_generic_s
{
	int			(*Init)(void);
	void		(*Update)(void);
	void		(*Set)(int property, float value);
	int			(*Get)(int property, void *value);
	void		(*Pause)(int pause);
	void		(*Stop)(void);
}
musinterface_generic_t;

// Driver interface for playing MUS music.
typedef struct musinterface_mus_s
{
	/*int			(*Init)(void);
	void		(*Set)(int property, float value);
	void		(*Pause)(int pause);*/
	musinterface_generic_t gen;
	void *		(*SongBuffer)(int length);
	int			(*Play)(int looped);
}
musinterface_mus_t;

// Driver interface for playing non-MUS music.
typedef struct musinterface_ext_s
{
	/*int			(*Init)(void);
	void		(*Set)(int property, float value);
	void		(*Pause)(int pause);*/
	musinterface_generic_t gen;
	void *		(*SongBuffer)(int length);
	int			(*PlayFile)(const char *filename, int looped);
	int			(*PlayBuffer)(int looped);
}
musinterface_ext_t;

// Driver interface for playing CD tracks.
typedef struct musinterface_cd_s
{
/*	int			(*Init)(void);
	void		(*Set)(int property, float value);
	void		(*Pause)(int pause);*/
	musinterface_generic_t gen;
	int			(*Play)(int track, int looped);
}
musinterface_cd_t;

#endif 