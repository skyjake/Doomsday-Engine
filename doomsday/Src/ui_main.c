/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * ui_main.c: Graphical User Interface
 *
 * Has ties to the console routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_ui.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define SCROLL_TIME		3

enum 
{
	UITEX_MOUSE,
	UITEX_CORNER,
	UITEX_FILL,
	UITEX_SHADE,
	UITEX_HINT,
	UITEX_LOGO,	
	NUM_UI_TEXTURES
};

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int		UI_ListItemHeight(uidata_list_t *listdata);
int		UI_ListButtonHeight(ui_object_t *ob);
int		UI_ListThumbPos(ui_object_t *ob);
void	UI_StrCpyLen(char *dest, char *src, int max_width);
int		UI_MouseInsideBox(int x, int y, int w, int h);
void	UI_MouseFocus(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int maxTexSize;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		ui_active = false;		// The user interface is active.
boolean		ui_showmouse = true;
ui_page_t	*ui_page = 0;			// Currently active page.
int			ui_fonthgt;				// Height of the UI font.
DGLuint		ui_textures[NUM_UI_TEXTURES]; // Cursor texture.
int			ui_cx, ui_cy;			// Cursor position.
int			ui_rest_cx, ui_rest_cy;
int			ui_rest_start;			// Start time of current resting.
int			ui_rest_time = TICSPERSEC/2;	// 500 ms.
int			ui_rest_offset_limit = 2;
int			ui_moved;				// True if the mouse has been moved.

int			uiMouseWidth = 16;
int			uiMouseHeight = 32;

// Modify these colors to change the look of the UI.
ui_color_t ui_colors[NUM_UI_COLORS] =
{
	/* UIC_TEXT */		{ 1, 1, 1 },
	/* UIC_SHADOW */	{ 0, 0, 0 },
	/* UIC_BG_LIGHT */	{ .18f, .18f, .22f },
	/* UIC_BG_MEDIUM */	{ .4f, .4f, .52f },
	/* UIC_BG_DARK */	{ .28f, .28f, .33f },
	/* UIC_BRD_HI */	{ 1, 1, 1 },
	/* UIC_BRD_MED */	{ 0, 0, 0 },
	/* UIC_BRD_LOW */	{ .25f, .25f, .55f },
	/* UIC_HELP */		{ .4f, .4f, .52f }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// UI_Init
//===========================================================================
void UI_Init(void)
{
	if(ui_active) return;
	ui_active = true;
	
	// Setup state.
	Con_StartupInit();
	
	// Change font.
	FR_SetFont(glFontVariable);
	ui_fonthgt = FR_TextHeight("W");

	// Should the mouse cursor be visible?
	ui_showmouse = !ArgExists("-nomouse");

	// Load graphics.
	
	ui_cx = screenWidth/2;
	ui_cy = screenHeight/2;
	ui_moved = false;
}

//===========================================================================
// UI_End
//===========================================================================
void UI_End(void)
{
	event_t rel;

	if(!ui_active) return;
	ui_active = false;
	// Restore old state.
	Con_StartupDone();

	// Inform everybody that the shift key was (possibly) released while
	// the UI was eating all the input events.
	if(!shiftDown)
	{
		rel.type = ev_keyup;
		rel.data1 = DDKEY_RSHIFT;
		DD_PostEvent(&rel);
	}
}

//===========================================================================
// UI_LoadTextures
//	Called from GL_LoadSystemTextures.
//===========================================================================
void UI_LoadTextures(void)
{
	int i;
	const char *picNames[NUM_UI_TEXTURES] =
	{
		"Mouse",
		"BoxCorner",
		"BoxFill",
		"BoxShade",
		"Hint",
		"Logo"
	};

	for(i = 0; i < NUM_UI_TEXTURES; i++)
		if(!ui_textures[i])
		{
			ui_textures[i] = GL_LoadGraphics(picNames[i], LGM_NORMAL);
		}
}

//===========================================================================
// UI_ClearTextures
//===========================================================================
void UI_ClearTextures(void)
{
	gl.DeleteTextures(NUM_UI_TEXTURES, ui_textures);
	memset(ui_textures, 0, sizeof(ui_textures));
}

//===========================================================================
// UI_InitPage
//===========================================================================
void UI_InitPage(ui_page_t *page, ui_object_t *objects)
{
	int i;
	ui_object_t *deffocus = NULL;
	ui_object_t meta;

	memset(page, 0, sizeof(*page));
	page->objects = objects;
	page->capture = -1;	// No capture.
	page->focus = -1;
	page->responder = UIPage_Responder;
	page->drawer = UIPage_Drawer;
	page->ticker = UIPage_Ticker;
	page->count = UI_CountObjects(objects);
	for(i = 0; i < page->count; i++)
	{
		objects[i].flags &= ~UIF_FOCUS;
		if(objects[i].type == UI_TEXT 
			|| objects[i].type == UI_BOX
			|| objects[i].type == UI_META) objects[i].flags |= UIF_NO_FOCUS;
		if(objects[i].flags & UIF_DEFAULT) deffocus = objects + i;
		// Reset timer.
		objects[i].timer = 0;
	}
	if(deffocus)
	{
		page->focus = deffocus - objects;
		deffocus->flags |= UIF_FOCUS;
	}
	else
	{
		// Find an object for focus.
		for(i=0; i<page->count; i++)
		{
			if(!(objects[i].flags & UIF_NO_FOCUS))
			{
				// Set default focus.
				page->focus = i;
				objects[i].flags |= UIF_FOCUS;
				break;
			}
		}
	}
	// Meta effects.
	for(i = 0, meta.type = 0; i < page->count; i++)
	{
		if(!meta.type && objects[i].type != UI_META) continue;
		if(objects[i].type == UI_META)
		{
			// This will be the meta for now.
			memcpy(&meta, objects + i, sizeof(meta));
			// Neutralize the actual object.
			objects[i].group = UIG_NONE;
			objects[i].flags |= UIF_HIDDEN;
			objects[i].relx = objects[i].rely = 0;
			objects[i].relw = objects[i].relh = 0;
			continue;
		}
		// Apply the meta.
		if(meta.group != UIG_NONE) objects[i].group = meta.group;
		objects[i].relx += meta.relx;
		objects[i].rely += meta.rely;
		objects[i].relw += meta.relw;
		objects[i].relh += meta.relh;
	}
}

//===========================================================================
// UI_AvailableWidth
//	The width of the available page area, in pixels.
//===========================================================================
int UI_AvailableWidth(void)
{
	return screenWidth - UI_BORDER*4;
}

//===========================================================================
// UI_AvailableHeight
//===========================================================================
int UI_AvailableHeight(void)
{
	return screenHeight - UI_TITLE_HGT - UI_BORDER*4;
}

//===========================================================================
// UI_ScreenX
//	Convert a relative coordinate to a screen coordinate.
//===========================================================================
int UI_ScreenX(int relx)
{
	return UI_BORDER*2 + (relx * UI_AvailableWidth()) / 1000;		
}

//===========================================================================
// UI_ScreenY
//	Convert a relative coordinate to a screen coordinate.
//===========================================================================
int UI_ScreenY(int rely)
{
	return UI_BORDER*2 + UI_TITLE_HGT + (rely * UI_AvailableHeight()) / 1000; 
}

//===========================================================================
// UI_ScreenW
//===========================================================================
int UI_ScreenW(int relw)
{
	return (relw * UI_AvailableWidth()) / 1000;
}

//===========================================================================
// UI_ScreenH
//===========================================================================
int UI_ScreenH(int relh)
{
	return (relh * UI_AvailableHeight()) / 1000;
}

//===========================================================================
// UI_SetPage
//	Change and prepare the active page.
//===========================================================================
void UI_SetPage(ui_page_t *page)
{
	int i;
	ui_object_t *ob;

	ui_page = page;	
	if(!page) return;
	// Init objects.
	for(i=0, ob=page->objects; i<page->count; i++, ob++) 
	{
		// Calculate real coordinates.
		ob->x = UI_ScreenX(ob->relx);
		ob->w = UI_ScreenW(ob->relw);
		ob->y = UI_ScreenY(ob->rely); 
		ob->h = UI_ScreenH(ob->relh);
		// Update edit box text.
		if(ob->type == UI_EDIT)
		{
			memset(ob->text, 0, sizeof(ob->text));
			strncpy(ob->text, ((uidata_edit_t*)ob->data)->ptr, 255);
		}
		// Stay-down button state.
		if(ob->type == UI_BUTTON2)
		{
			if( *(char*) ob->data)
				ob->flags |= UIF_ACTIVE;
			else
				ob->flags &= ~UIF_ACTIVE;
		}
		// List box number of visible items.
		if(ob->type == UI_LIST)
		{
			uidata_list_t *dat = ob->data;
			dat->numvis = (ob->h - 2*UI_BORDER) / UI_ListItemHeight(dat);
			if(dat->selection < dat->first)
				dat->first = dat->selection;
			if(dat->selection > dat->first+dat->numvis-1)
				dat->first = dat->selection - dat->numvis + 1;
			UI_InitColumns(ob);
		}
	}
	// The mouse has not yet been moved on this page.
	ui_moved = false;
}

int UI_Responder(event_t *ev)
{	
	if(!ui_active) return false;
	if(!ui_page) return false;
	// Check for Shift events.
	switch(ev->type)
	{
	case ev_mouse:
		if(ev->data1 || ev->data2) ui_moved = true;
		ui_cx += ev->data1;
		ui_cy += ev->data2;
		if(ui_cx < 0) ui_cx = 0;
		if(ui_cy < 0) ui_cy = 0;
		if(ui_cx >= screenWidth) ui_cx = screenWidth-1;
		if(ui_cy >= screenHeight) ui_cy = screenHeight-1;
		break;

	default:
		break;
	}
	// Call the page's responder.
	ui_page->responder(ui_page, ev);
	// If the UI is active, all events are eaten by it.
	return true;
}

void UI_Ticker(timespan_t time)
{
	static trigger_t fixed = { 1/35.0 };

	if(!ui_active) return;
	if(!ui_page) return;
	if(!M_CheckTrigger(&fixed, time)) return;

	// Call the active page's ticker.
	ui_page->ticker(ui_page);
}

void UI_Drawer(void)
{
	if(!ui_active) return;
	if(!ui_page) return;
	// Call the active page's drawer.
	ui_page->drawer(ui_page);
	// Draw mouse cursor.
	UI_DrawMouse(ui_cx, ui_cy);
}

int UI_CountObjects(ui_object_t *list)
{
	int count;

	for(count=0; list->type != UI_NONE; list++, count++);
	return count;
}

//===========================================================================
// UI_FlagGroup
//===========================================================================
void UI_FlagGroup(ui_object_t *list, int group, int flags, int set)
{
	for(; list->type; list++)
		if(list->group == group)
		{
			switch(set)
			{
			case UIFG_CLEAR:
				list->flags &= ~flags;
				break;
				
			case UIFG_SET:
				list->flags |= flags;
				break;
				
			case UIFG_XOR:
				list->flags ^= flags;
				break;
			}
		}
}

//===========================================================================
// UI_FindObject
//	All the specified flags must be set.
//===========================================================================
ui_object_t *UI_FindObject(ui_object_t *list, int group, int flags)
{
	for(; list->type; list++)
		if(list->group == group
			&& (list->flags & flags) == flags) return list;
	return NULL;
}

//===========================================================================
// UI_MouseFocus
//	Set focus to the object under the mouse cursor.
//===========================================================================
void UI_MouseFocus(void)
{
	int i;
	ui_object_t *ob;

	for(i = 0, ob = ui_page->objects; i < ui_page->count; i++, ob++)
		if(!(ob->flags & UIF_NO_FOCUS) && UI_MouseInside(ob))
		{
			UI_Focus(ob);
			break;
		}
}

// Ob must be on the current page! It can't be NULL.
void UI_Focus(ui_object_t *ob)
{
	int i;

	if(!ob) Con_Error("UI_Focus: Tried to set focus on NULL.\n");

	// Can the object receive focus?
	if(ob->flags & UIF_NO_FOCUS) return;
	ui_page->focus = ob - ui_page->objects;
	for(i = 0; i < ui_page->count; i++)
	{
		if(i == ui_page->focus)
			ui_page->objects[i].flags |= UIF_FOCUS;
		else
			ui_page->objects[i].flags &= ~UIF_FOCUS;
	}
}

// Ob must be on the current page!
// If ob is NULL, capture is ended.
void UI_Capture(ui_object_t *ob)
{
	if(!ob)
	{
		// End capture.
		ui_page->capture = -1;
		return;
	}
	if(!ob->responder) return;	// Sorry, pal...
	// Set the capture object.
	ui_page->capture = ob - ui_page->objects;
	// Set focus.
	UI_Focus(ob);
}

//---------------------------------------------------------------------------
// Default Callback Functions
//---------------------------------------------------------------------------

int UIPage_Responder(ui_page_t *page, event_t *ev)
{
	int i, k;
	ui_object_t *ob;
	event_t translated;

	// Translate mouse wheel?
	if(ev->type == ev_mousebdown)
	{
		if(ev->data1 & (DDMB_MWHEELUP | DDMB_MWHEELDOWN))
		{
			UI_MouseFocus();
			translated.type = ev_keydown;
			translated.data1 = (ev->data1 & DDMB_MWHEELUP? DDKEY_UPARROW
				: DDKEY_DOWNARROW);
			ev = &translated;
		}
	}

	if(page->capture >= 0)
	{
		// There is an object that has captured input.
		ob = page->objects + page->capture;
		// Capture objects must have a responder!
		// This object gets to decide what happens.
		return ob->responder(ob, ev);
	}

	// Check for Esc key.
	if(ev->type == ev_keydown || ev->type == ev_keyrepeat)
	{
		// We won't accept repeats with Esc.
		if(ev->data1 == DDKEY_ESCAPE && ev->type == ev_keydown)
		{
			UI_SetPage(page->previous);
			// If we have no more a page, disactive UI.
			if(!ui_page) UI_End();
			return true;	// The event was used.
		}
		// Tab is used for navigation.
		if(ev->data1 == DDKEY_TAB)
		{
			// Remove the focus flag from the current focus object.
			page->objects[page->focus].flags &= ~UIF_FOCUS;
			// Move focus.
			k = 0;
			do
			{
				page->focus += shiftDown? -1 : 1;
				// Check range.
				if(page->focus < 0) 
					page->focus = page->count - 1;
				else if(page->focus >= page->count)
					page->focus = 0;
			}					
			while(++k < page->count && 
				(page->objects[page->focus].flags & 
				(UIF_DISABLED|UIF_NO_FOCUS|UIF_HIDDEN)));
			// Flag the new focus object.
			page->objects[page->focus].flags |= UIF_FOCUS;
			return true;	// The event was used.
		}
	}
	// Call responders until someone uses the event.
	// We start with the focus object.
	for(i = 0; i < page->count; i++)
	{
		// Determine the index of the object to process.
		k = page->focus + i;
		// Wrap around.
		if(k < 0) k += page->count;
		if(k >= page->count) k -= page->count;	
		ob = page->objects + k;
		// Check the flags of this object.
		if(ob->flags & UIF_HIDDEN || ob->flags & UIF_DISABLED)
			continue;	// These flags prevent response.
		if(!ob->responder) continue;	// Must have a responder.
		if(ob->responder(ob, ev))
		{
			// The event was used by this object.
			UI_Focus(ob);	// Move focus to it.
			return true;
		}
	}
	// We didn't use the event.
	return false;
}

//===========================================================================
// UIPage_Ticker
//	Call the ticker routine for each object.
//===========================================================================
void UIPage_Ticker(ui_page_t *page)
{
	int i;
	ui_object_t *ob;

	// Call the ticker of each object, unless they're hidden or paused.
	for(i = 0, ob = page->objects; i < page->count; i++, ob++)
	{
		if(ob->flags & UIF_PAUSED || ob->flags & UIF_HIDDEN) continue;
		if(ob->ticker) ob->ticker(ob);
		// Advance object timer.
		ob->timer++;
	}
	page->timer++;

	// Check mouse resting.
	if(abs(ui_cx - ui_rest_cx) > ui_rest_offset_limit
		|| abs(ui_cy - ui_rest_cy) > ui_rest_offset_limit)
	{
		// Restart resting period.
		ui_rest_cx = ui_cx;
		ui_rest_cy = ui_cy;
		ui_rest_start = page->timer;
	}
}

void UIPage_Drawer(ui_page_t *page)
{
	int i;
	float t;
	ui_object_t *ob;
	ui_color_t focuscol;

	// Draw background.
	Con_DrawStartupBackground();
	// Draw title.
	UI_DrawTitle(page);
	// Draw each object, unless they're hidden.
	for(i = 0, ob = page->objects; i < page->count; i++, ob++)
	{
		if(ob->flags & UIF_HIDDEN || !ob->drawer) continue;
		ob->drawer(ob);		
		if(ob->flags & UIF_FOCUS && (ob->type != UI_EDIT 
			|| !(ob->flags & UIF_ACTIVE)))
		{
			t = (1 + sin(page->timer / (float)TICSPERSEC * 1.5f * PI)) / 2;
			UI_MixColors(UI_COL(UIC_BRD_LOW), UI_COL(UIC_BRD_HI), 
				&focuscol, t);		
			UI_Shade(ob->x, ob->y, ob->w, ob->h, UI_BORDER, 
				UI_COL(UIC_BRD_LOW), UI_COL(UIC_BRD_LOW), 
				.2f + t*.3f, -1);
			gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
			// Draw a focus rectangle.
			UI_DrawRect(ob->x-1, ob->y-1, ob->w+2, ob->h+2, UI_BORDER, 
				&focuscol, 1);
			gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		}
	}
}

void UIFrame_Drawer(ui_object_t *ob)
{
	int b = UI_BORDER;

	UI_GradientEx(ob->x, ob->y, ob->w, ob->h, b,
		UI_COL(UIC_BG_MEDIUM), 0, .6f, 0);
	UI_DrawRect(ob->x, ob->y, ob->w, ob->h, b, 
		UI_COL(UIC_BRD_HI) /*, UI_COL(UIC_BRD_MED), UI_COL(UIC_BRD_LOW)*/, 1);
}

void UIText_Drawer(ui_object_t *ob)
{
	UI_TextOutEx(ob->text, ob->x, ob->y + ob->h/2, false, true, 
		UI_COL(UIC_TEXT), ob->flags & UIF_DISABLED? .2f : 1);
}

int UIButton_Responder(ui_object_t *ob, event_t *ev)
{
	if(ob->flags & UIF_CLICKED)
	{
		if(ev->type == ev_mousebup || ev->type == ev_keyup)
		{
			UI_Capture(0);
			ob->flags &= ~UIF_CLICKED;		
			if(UI_MouseInside(ob) || ev->type == ev_keyup)
			{
				// Activate?
				if(ob->action) ob->action(ob);
			}
			return true;
		}
	}
	else if((ev->type == ev_mousebdown && UI_MouseInside(ob)) ||
		(ev->type == ev_keydown && IS_ACTKEY(ev->data1)))
	{
		if(ob->type == UI_BUTTON)
		{
			// Capture input.
			UI_Capture(ob);
			ob->flags |= UIF_CLICKED;
		}
		else
		{
			// Stay-down buttons change state.
			ob->flags ^= UIF_ACTIVE;
			if(ob->data) *(char*) ob->data = (ob->flags & UIF_ACTIVE) != 0;
			// Call the action function.
			if(ob->action) ob->action(ob);
		}
		ob->timer = 0;
		return true;
	}
	return false;
}

void UIButton_Drawer(ui_object_t *ob)
{
	int dis = (ob->flags & UIF_DISABLED) != 0;
	int act = (ob->flags & UIF_ACTIVE) != 0;
	int click = (ob->flags & UIF_CLICKED) != 0;
	boolean down = act || click;
	ui_color_t back;
	float t = ob->timer/15.0f;
	float alpha = (dis? .2f : 1);

	// Mix the background color.
	if(!click || t > .5f) t = .5f;
	if(act && t > .1f) t = .1f;
	UI_MixColors(UI_COL(UIC_TEXT), UI_COL(UIC_SHADOW), &back, t);
	UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BUTTON_BORDER, &back, 0, 
		alpha, 0);
	UI_Shade(ob->x, ob->y, ob->w, ob->h, UI_BUTTON_BORDER * (down? -1 : 1), 
		UI_COL(UIC_BRD_HI), UI_COL(UIC_BRD_LOW), alpha/3, -1);
	UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, 
		UI_BUTTON_BORDER * (down? -1 : 1), 
		false, UI_COL(UIC_BRD_HI), NULL, alpha, -1);
	UI_TextOutEx(ob->text, down + ob->x 
		+ (ob->flags & UIF_LEFT_ALIGN? UI_BUTTON_BORDER*2 : ob->w/2), 
		down + ob->y + ob->h/2,
		!(ob->flags & UIF_LEFT_ALIGN), true, 
		UI_COL(UIC_TEXT), alpha);
}

int UIEdit_Responder(ui_object_t *ob, event_t *ev)
{
	uidata_edit_t *dat = ob->data;

	if(ob->flags & UIF_ACTIVE)
	{
		if(ev->type != ev_keydown && ev->type != ev_keyrepeat) return false;
		switch(ev->data1)
		{
		case DDKEY_LEFTARROW:
			if(dat->cp > 0) dat->cp--;
			break;

		case DDKEY_RIGHTARROW:
			if(dat->cp < (int)strlen(ob->text)) dat->cp++;
			break;

		case DDKEY_HOME:
			dat->cp = 0;
			break;

		case DDKEY_END:
			dat->cp = strlen(ob->text);
			break;

		case DDKEY_BACKSPACE:
			if(!dat->cp) break;
			dat->cp--;

		case DDKEY_DEL:
			memmove(ob->text + dat->cp, ob->text + dat->cp + 1,
				strlen(ob->text) - dat->cp);
			break;

		case DDKEY_ENTER:
			// Store changes.
			memset(dat->ptr, 0, dat->maxlen+1);
			strncpy(dat->ptr, ob->text, dat->maxlen);
			if(ob->action) ob->action(ob);

		case DDKEY_ESCAPE:
			memset(ob->text, 0, sizeof(ob->text));
			strncpy(ob->text, dat->ptr, 255);
			ob->flags &= ~UIF_ACTIVE;
			UI_Capture(0);
			break;

		default:
			if( (int) strlen(ob->text) < dat->maxlen && ev->data1 >= 32 
				&& (ev->data1 <= 127 || ev->data1 >= DD_HIGHEST_KEYCODE))
			{
				memmove(ob->text + dat->cp + 1, ob->text + dat->cp,
					strlen(ob->text) - dat->cp);
				ob->text[dat->cp++] = DD_ModKey(ev->data1);
			}
			break;
		}
		return true;
	}
	else if((ev->type == ev_mousebdown && UI_MouseInside(ob)) ||
			(ev->type == ev_keydown && IS_ACTKEY(ev->data1)))
	{
		// Activate and capture.
		ob->flags |= UIF_ACTIVE;
		ob->timer = 0;
		UI_Capture(ob);
		memset(ob->text, 0, sizeof(ob->text));
		strncpy(ob->text, dat->ptr, 255);
		dat->cp = strlen(ob->text);
		return true;
	}		
	return false;
}

void UIEdit_Drawer(ui_object_t *ob)
{
	uidata_edit_t *dat = ob->data;
	int act = (ob->flags & UIF_ACTIVE) != 0;
	int dis = (ob->flags & UIF_DISABLED) != 0;
	ui_color_t back;
	float t = ob->timer/8.0f;
	char buf[256];
	int curx, i, maxw = ob->w - UI_BORDER*4, first_in_buf = 0;
	float alpha = (dis? .2f : .5f);

	// Mix the background color.
	if(!act || t > 1) t = 1;
	UI_MixColors(UI_COL(UIC_TEXT), UI_COL(UIC_SHADOW), &back, t);
	UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BORDER, 
		&back, 0, alpha, 0);
	UI_Shade(ob->x, ob->y, ob->w, ob->h, UI_BORDER, 
		UI_COL(UIC_BRD_HI), UI_COL(UIC_BRD_LOW), alpha/3, -1);
	UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, UI_BORDER * (act? -1 : 1), 
		false, UI_COL(UIC_BRD_HI), NULL, dis? .2f : 1, -1);
	// Draw text.
	memset(buf, 0, sizeof(buf));
	// Does all of it fit in the box?
	if(FR_TextWidth(ob->text) > maxw)
	{
		// No, it doesn't fit.
		if(!act)
			UI_StrCpyLen(buf, ob->text, maxw);
		else
		{
			// Can we show to the cursor?
			for(curx=0, i=0; i<dat->cp; i++)
				curx += FR_CharWidth(ob->text[i]);
			// How much do we need to skip forward?
			for(; curx > maxw; first_in_buf++)
				curx -= FR_CharWidth(ob->text[first_in_buf]);
			UI_StrCpyLen(buf, ob->text + first_in_buf, maxw);
		}
	}
	else
	{
		// It fits!
		strcpy(buf, ob->text);
	}
	UI_TextOutEx(buf, ob->x + UI_BORDER*2, ob->y + ob->h/2,
		false, true, UI_COL(UIC_TEXT), dis? .2f : 1);
	if(act && ob->timer & 4)
	{
		// Draw cursor.
		// Determine position.
		for(curx=0, i=first_in_buf; i<dat->cp; i++)
			curx += FR_CharWidth(ob->text[i]);
		UI_Gradient(ob->x + UI_BORDER*2 + curx-1, ob->y + ob->h/2 - ui_fonthgt/2,
			2, ui_fonthgt, UI_COL(UIC_TEXT), 0, 1, 1);
	}
}

int UIList_Responder(ui_object_t *ob, event_t *ev)
{
	uidata_list_t *dat = ob->data;
	int i, oldsel = dat->selection, buth, barh;
	int used = false;

	if(ob->flags & UIF_CLICKED)
	{
		// We've captured all input.
		if(ev->type == ev_mousebup)
		{
			dat->button[1] = false;
			// Release capture.
			UI_Capture(0);
			ob->flags &= ~UIF_CLICKED;
		}
		if(ev->type == ev_mouse)
		{
			// Calculate the new position.
			buth = UI_ListButtonHeight(ob);
			barh = ob->h - 2*(UI_BORDER + buth);
			if(barh - buth)
			{
				dat->first = ((ui_cy - ob->y - UI_BORDER - (buth*3)/2) * 
					(dat->count - dat->numvis) + (barh-buth)/2) / (barh-buth);
			}
			else
			{
				dat->first = 0;
			}
			// Check that it's in range.
			if(dat->first < 0) dat->first = 0;
			if(dat->first > dat->count - dat->numvis)
				dat->first = dat->count - dat->numvis;
		}
		// We're eating everything.
		return true;
	}
	else if(ev->type == ev_keydown || ev->type == ev_keyrepeat)
	{
		used = true;
		switch(ev->data1)
		{
		case DDKEY_UPARROW:
			if(dat->selection > 0) dat->selection--;
			break;

		case DDKEY_DOWNARROW:
			if(dat->selection < dat->count-1) dat->selection++;
			break;
		
		case DDKEY_HOME:
			dat->selection = 0;
			break;

		case DDKEY_END:
			dat->selection = dat->count-1;
			break;

		default:
			used = false;
		}
	}
	else if(ev->type == ev_mousebdown)
	{
		if(!UI_MouseInside(ob)) return false;
		// Now we know we're going to eat this event.
		used = true;
		buth = UI_ListButtonHeight(ob);
		// Clicked in the item section?
		if(UI_MouseInsideBox(ob->x + UI_BORDER, ob->y + UI_BORDER,
			ob->w - 2*UI_BORDER - (dat->count >= dat->numvis? UI_BAR_WDH : 0), 
			ob->h - 2*UI_BORDER))
		{
			dat->selection = dat->first + (ui_cy - ob->y - UI_BORDER) / 
				UI_ListItemHeight(dat);
			if(dat->selection >= dat->count) dat->selection = dat->count-1;
		}
		else if(dat->count < dat->numvis)
		{
			// No scrollbar.
			return true;
		}
		// Clicked the Up button?
		else if(UI_MouseInsideBox(ob->x + ob->w - UI_BORDER - UI_BAR_WDH,
			ob->y + UI_BORDER, UI_BAR_WDH, buth))
		{
			// The Up button is now pressed.
			dat->button[0] = true;
			ob->timer = SCROLL_TIME;	// Ticker does the scrolling.
			return true;
		}
		// Clicked the Down button?
		else if(UI_MouseInsideBox(ob->x + ob->w - UI_BORDER - UI_BAR_WDH,
			ob->y + ob->h - UI_BORDER - buth, UI_BAR_WDH, buth))
		{
			// The Down button is now pressed.
			dat->button[2] = true;
			ob->timer = SCROLL_TIME;	// Ticker does the scrolling.
			return true;
		}
		// Clicked the Thumb?
		else if(UI_MouseInsideBox(ob->x + ob->w - UI_BORDER - UI_BAR_WDH,
			UI_ListThumbPos(ob), UI_BAR_WDH, buth))
		{
			dat->button[1] = true;
			// Capture input and start tracking mouse movement.
			UI_Capture(ob);
			ob->flags |= UIF_CLICKED;
			return true;
		}
		else return false;
	}
	else if(ev->type == ev_mousebup)
	{
		// Release all buttons.
		for(i = 0; i < 3; i++) dat->button[i] = false;
		// We might have leaved the object's area, so let other objects
		// process this event, too.
		return false;	
	}
	else 
	{
		// We are not going to use this event.
		return false;
	}
	// Adjust the first visible item.
	if(dat->selection < dat->first)
		dat->first = dat->selection;
	if(dat->selection > dat->first+dat->numvis-1)
		dat->first = dat->selection - dat->numvis + 1;
	// Call action function?
	if(oldsel != dat->selection && ob->action) ob->action(ob);
	return used;
}

void UIList_Ticker(ui_object_t *ob)
{
	uidata_list_t *dat = ob->data;

	if(ob->timer >= SCROLL_TIME && (dat->button[0] || dat->button[2]))
	{
		// Next time in 3 ticks.
		ob->timer = 0;
		// Send a direct event.
		if(dat->button[0] && dat->first > 0) dat->first--;
		if(dat->button[2] && dat->first < dat->count-dat->numvis) dat->first++;
	}
}

void UIList_Drawer(ui_object_t *ob)
{
	uidata_list_t *dat = ob->data;
	uidata_listitem_t *items = dat->items;
	int dis = (ob->flags & UIF_DISABLED) != 0;
	int i, c, x, y, ihgt, maxw = ob->w - 2*UI_BORDER;
	int maxh = ob->h - 2*UI_BORDER, buth;
	char buf[256], *ptr, *endptr, tmp[256];
	float alpha = dis? .2f : 1;
	int barw;

	// The background.
	UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BORDER,
		UI_COL(UIC_SHADOW), 0, alpha/2, 0);
	// The borders.
	UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, -UI_BORDER, false,
		UI_COL(UIC_BRD_HI), NULL, alpha, -1);
	// The title.
	UI_TextOutEx(ob->text, ob->x, ob->y - UI_BORDER - ui_fonthgt, false, false,
		UI_COL(UIC_TEXT), alpha);
	// Is a scroll bar necessary?
	ihgt = UI_ListItemHeight(dat);
	if(dat->numvis < dat->count)
	{
		barw = UI_BAR_WDH;
		maxw -= barw;
		buth = UI_ListButtonHeight(ob);
		x = ob->x + ob->w - UI_BORDER - barw;
		y = ob->y + UI_BORDER;
		UI_GradientEx(x, y, barw, maxh, UI_BAR_BUTTON_BORDER, 
			UI_COL(UIC_TEXT), 0, alpha*.2f, alpha*.2f);
		// Up Button.	
		UI_DrawButton(x, y, barw, buth, UI_BAR_BUTTON_BORDER,
			!dat->first? alpha*.2f : alpha, NULL,
			dat->button[0], dis, UIBA_UP);
		// Thumb Button.
		UI_DrawButton(x, UI_ListThumbPos(ob), barw, buth, UI_BAR_BUTTON_BORDER,
			alpha, NULL, dat->button[1], dis, UIBA_NONE);
		// Down Button.
		UI_DrawButton(x, y+maxh-buth, barw, buth, UI_BAR_BUTTON_BORDER,
			dat->first+dat->numvis>=dat->count? alpha*.2f : alpha, NULL,
			dat->button[2], dis, UIBA_DOWN);
	}
	x = ob->x + UI_BORDER;
	y = ob->y + UI_BORDER;
	// Draw columns?
	for(c=0; c<UI_MAX_COLUMNS; c++)
	{
		if(!dat->column[c] || dat->column[c] > maxw-2*UI_BORDER) continue;
		UI_Gradient(x + UI_BORDER + dat->column[c] - 2,
			ob->y + UI_BORDER, 1, maxh, UI_COL(UIC_TEXT), 0, 
			alpha*.5f, alpha*.5f);
	}
	for(i=dat->first; i<dat->count && i<dat->first+dat->numvis; i++, y+=ihgt)
	{
		// The selection has a white background.
		if(i == dat->selection)
		{
			UI_GradientEx(x, y, maxw, ihgt, UI_BAR_BORDER,
				UI_COL(UIC_TEXT), 0, alpha*.6f, alpha*.2f);
		}
		// The text, clipped w/columns.
		ptr = items[i].text;
		for(c=0; c<UI_MAX_COLUMNS; c++)
		{
			endptr = strchr(ptr, '\t');
			memset(tmp, 0, sizeof(tmp));
			if(endptr) strncpy(tmp, ptr, endptr-ptr); else strcpy(tmp, ptr);
			memset(buf, 0, sizeof(buf));
			UI_StrCpyLen(buf, tmp, maxw-2*UI_BORDER - dat->column[c]);
			UI_TextOutEx(buf, x + UI_BORDER + dat->column[c], y + ihgt/2, 
				false, true, UI_COL(UIC_TEXT), alpha);
			if(!endptr) break;
			ptr = endptr + 1;
		}
	}
}

//===========================================================================
// UI_SliderButtonWidth
//===========================================================================
int UI_SliderButtonWidth(ui_object_t *ob)
{
//	uidata_slider_t *dat = ob->data;
	int width = ob->h - UI_BAR_BORDER*2;
	if(width < UI_BAR_BORDER*3) width = UI_BAR_BORDER*3;
	return width;
}

//===========================================================================
// UI_SliderThumbPos
//===========================================================================
int UI_SliderThumbPos(ui_object_t *ob)
{
	uidata_slider_t *dat = ob->data;
	float range = dat->max - dat->min, useval;
	int butw = UI_SliderButtonWidth(ob);

	if(!range) range = 1;	// Should never happen.
	if(dat->floatmode)
		useval = dat->value;
	else
		useval = (int) (dat->value + 0.5f);
	useval -= dat->min;
	return ob->x + UI_BAR_BORDER + butw
		+ useval/range*(ob->w - UI_BAR_BORDER*2 - butw*3);
}

//===========================================================================
// UISlider_Responder
//===========================================================================
int UISlider_Responder(ui_object_t *ob, event_t *ev)
{
	uidata_slider_t *dat = ob->data;
	float oldvalue = dat->value;
	boolean used = false;
	int i, butw, inw;

	if(ob->flags & UIF_CLICKED)
	{
		// We've captured all input.
		if(ev->type == ev_mousebup)
		{
			dat->button[1] = false; // Release thumb.
			UI_Capture(0);
			ob->flags &= ~UIF_CLICKED;
		}
		if(ev->type == ev_mouse)
		{
			// Calculate new value from the mouse position.
			butw = UI_SliderButtonWidth(ob);
			inw = ob->w - 2*UI_BAR_BORDER - 3*butw;
			if(inw > 0)
			{
				dat->value = dat->min + (dat->max - dat->min)
					* (ui_cx - ob->x - UI_BAR_BORDER - (3*butw)/2)/(float)inw;
			}
			else
			{
				dat->value = dat->min;
			}
			if(dat->value < dat->min) dat->value = dat->min;
			if(dat->value > dat->max) dat->value = dat->max;
			if(!dat->floatmode) dat->value = (int) (dat->value + .5f);
			if(ob->action) ob->action(ob);
		}
		return true;
	}
	else if(ev->type == ev_keydown || ev->type == ev_keyrepeat)
	{
		used = true;
		switch(ev->data1)
		{
		case DDKEY_HOME:
			dat->value = dat->min;
			break;

		case DDKEY_END:
			dat->value = dat->max;
			break;

		case DDKEY_LEFTARROW:
			dat->value -= dat->step;
			if(dat->value < dat->min) dat->value = dat->min;
			break;

		case DDKEY_RIGHTARROW:
			dat->value += dat->step;
			if(dat->value > dat->max) dat->value = dat->max;
			break;

		default:
			used = false;
		}
	}
	else if(ev->type == ev_mousebdown)
	{
		if(!UI_MouseInside(ob)) return false;
		used = true;
		butw = UI_SliderButtonWidth(ob);
		// Where is the mouse cursor?
		if(UI_MouseInsideBox(ob->x, ob->y, butw + UI_BAR_BORDER, ob->h))
		{
			// The Left button is now pressed.
			dat->button[0] = true;
			ob->timer = SCROLL_TIME;	// Ticker does the scrolling.
			return true;
		}
		if(UI_MouseInsideBox(ob->x + ob->w - butw - UI_BAR_BORDER, ob->y,
			butw + UI_BAR_BORDER, ob->h))
		{
			// The Right button is now pressed.
			dat->button[2] = true;
			ob->timer = SCROLL_TIME;	// Tickes does the scrolling.
			return true;
		}
		if(UI_MouseInsideBox(UI_SliderThumbPos(ob), ob->y, butw, ob->h))
		{
			// Capture input and start tracking mouse movement.
			dat->button[1] = true;
			UI_Capture(ob);
			ob->flags |= UIF_CLICKED;
			return true;
		}
	}
	else if(ev->type == ev_mousebup)
	{
		// Release all buttons.
		for(i = 0; i < 3; i++) dat->button[i] = false;
		// We might be outside the object's area (or none of the buttons
		// might even be pressed), so let others have this event, too.
		return false;
	}
	else
	{
		// We are not going to use this event.
		return false;
	}
	// Did the value change?
	if(oldvalue != dat->value && ob->action) ob->action(ob);
	return used;
}

//===========================================================================
// UISlider_Ticker
//===========================================================================
void UISlider_Ticker(ui_object_t *ob)
{
	uidata_slider_t *dat = ob->data;
	float oldval;

	if(ob->timer >= SCROLL_TIME && (dat->button[0] || dat->button[2]))
	{
		ob->timer = 0;
		oldval = dat->value;
		if(dat->button[0]) dat->value -= dat->step;
		if(dat->button[2]) dat->value += dat->step;
		if(dat->value < dat->min) dat->value = dat->min;
		if(dat->value > dat->max) dat->value = dat->max;
		if(oldval != dat->value && ob->action) ob->action(ob);
	}
}

//===========================================================================
// UISlider_Drawer
//===========================================================================
void UISlider_Drawer(ui_object_t *ob)
{
	uidata_slider_t *dat = ob->data;
	boolean dis = (ob->flags & UIF_DISABLED) != 0;
	int inwidth = ob->w - UI_BAR_BORDER*2, inheight = ob->h - UI_BAR_BORDER*2;
	int butw = UI_SliderButtonWidth(ob);
	int butbor = UI_BAR_BUTTON_BORDER;
	int x, y, thumbx;
	float alpha = dis? .2f : 1;
	char buf[80];

	// The background.
	UI_GradientEx(ob->x, ob->y, ob->w, ob->h, UI_BAR_BORDER,
		UI_COL(UIC_SHADOW), 0, alpha/2, 0);

	// The borders.
	UI_DrawRectEx(ob->x, ob->y, ob->w, ob->h, -UI_BAR_BORDER, false,
		UI_COL(UIC_BRD_HI), NULL, alpha, -1);

	x = ob->x + UI_BAR_BORDER;
	y = ob->y + UI_BAR_BORDER;

	// The left button.
	UI_DrawButton(x, y, butw, inheight, butbor, 
		alpha * (dat->value == dat->min? .2f : 1), NULL, 
		dat->button[0], dis, UIBA_LEFT);

	// The right button.
	UI_DrawButton(x + inwidth - butw, y, butw, inheight, butbor,
		alpha * (dat->value == dat->max? .2f : 1), NULL, 
		dat->button[2], dis, UIBA_RIGHT);

	// The thumb.
	UI_DrawButton(thumbx = UI_SliderThumbPos(ob), y, butw, inheight, 
		butbor, alpha, NULL, dat->button[1], dis, UIBA_NONE);

	// The value.
	if(dat->floatmode)
		sprintf(buf, "%.2f", dat->value);
	else
		sprintf(buf, "%i", (int)dat->value);
	if(dat->zerotext && dat->value == dat->min) 
		strcpy(buf, dat->zerotext);
	UI_TextOutEx(buf, x + (dat->value < (dat->min + dat->max)/2?
		inwidth - butw - UI_BAR_BORDER - FR_TextWidth(buf) 
		: butw + UI_BAR_BORDER), y + inheight/2, false, true, 
		UI_COL(UIC_TEXT), alpha);
}

//---------------------------------------------------------------------------
// Helper Functions
//---------------------------------------------------------------------------

void UI_InitColumns(ui_object_t *ob)
{
	uidata_list_t *dat = ob->data;
	uidata_listitem_t *list = dat->items;
	int i, c, w, sep, last, maxw;
	char *endptr, *ptr, temp[256];
	int width[UI_MAX_COLUMNS];
	int numcols = 1;

	memset(dat->column, 0, sizeof(dat->column));
	memset(width, 0, sizeof(width));
	for(i=0; i<dat->count; i++)
	{
		ptr = list[i].text;
		for(c=0; c<UI_MAX_COLUMNS; c++)
		{
			if(c+1 > numcols) numcols = c + 1;
			endptr = strchr(ptr, '\t');
			memset(temp, 0, sizeof(temp));
			if(endptr) 
				strncpy(temp, ptr, endptr-ptr);
			else
				strcpy(temp, ptr);
			w = FR_TextWidth(temp);
			if(w > width[c]) width[c] = w;
			if(!endptr) break;
			ptr = endptr + 1;
		}
	}
	// Calculate the total maximum width.
	for(w=i=0; i<UI_MAX_COLUMNS; i++) 
	{
		w += width[i];		
		if(width[i]) last = width[i];
	}
	// Calculate the offset for each column.
	maxw = ob->w - 4*UI_BORDER - (dat->count > dat->numvis? UI_BAR_WDH : 0);
	sep = maxw - w;
	if(numcols > 1) sep /= numcols - 1;
	if(sep < 0) sep = 0;
	for(c=i=0; i<numcols; i++)
	{
		dat->column[i] = c;
		c += sep + width[i];
	}
}

int UI_ListItemHeight(uidata_list_t *listdata)
{
	int h = listdata->itemhgt;
	if(h < ui_fonthgt) h = ui_fonthgt;
	return h;
}

int UI_ListButtonHeight(ui_object_t *ob)
{
	int barh = ob->h - 2*UI_BORDER;
	int buth = UI_BAR_WDH;
	if(buth > barh/3) buth = barh/3;
	return buth;
}

int UI_ListThumbPos(ui_object_t *ob)
{
	uidata_list_t *dat = ob->data;
	int buth = UI_ListButtonHeight(ob);
	int barh = ob->h - 2*(UI_BORDER + buth);

	if(dat->count <= dat->numvis) return 0;
	return ob->y + UI_BORDER + buth + ((barh-buth) * dat->first) /
		(dat->count - dat->numvis);
}

int UI_ListFindItem(ui_object_t *ob, int data_value)
{
	uidata_list_t *dat = ob->data;
	int i;

	for(i = 0; i < dat->count; i++)
		if(((uidata_listitem_t*)dat->items)[i].data == data_value)
			return i;
	return -1;
}

void UI_StrCpyLen(char *dest, char *src, int max_width)
{
	int i, width;
	
	for(i=0, width=0; src[i]; i++)
	{
		dest[i] = src[i];
		width += FR_CharWidth(src[i]);
		if(width > max_width)
		{
			dest[i] = 0;
			break;
		}
	}
}

int UI_MouseInsideBox(int x, int y, int w, int h)
{
	return (ui_cx >= x && ui_cx <= x+w && ui_cy >= y && ui_cy <= y+h);
}

//===========================================================================
// UI_MouseInside
//	Returns true if the mouse is inside the object.
//===========================================================================
int UI_MouseInside(ui_object_t *ob)
{
	return UI_MouseInsideBox(ob->x, ob->y, ob->w, ob->h);
}

//===========================================================================
// UI_MouseResting
//	Returns true if the mouse hasn't been moved for a while.
//===========================================================================
int UI_MouseResting(ui_page_t *page)
{
	if(!ui_moved) return false;
	return page->timer - ui_rest_start >= ui_rest_time;
}

void UI_MixColors(ui_color_t *a, ui_color_t *b, ui_color_t *dest, float amount)
{
	dest->red = (1-amount)*a->red + amount*b->red;
	dest->green = (1-amount)*a->green + amount*b->green;
	dest->blue = (1-amount)*a->blue + amount*b->blue;	
}

void UI_ColorA(ui_color_t *color, float alpha)
{
	gl.Color4f(color->red, color->green, color->blue, alpha);
}

void UI_Color(ui_color_t *color)
{
	gl.Color3f(color->red, color->green, color->blue);
}

void UI_DrawTitleEx(char *text, int height)
{
	UI_Gradient(0, 0, screenWidth, height, 
		UI_COL(UIC_BG_MEDIUM), UI_COL(UIC_BG_LIGHT), .8f, 1);
	UI_Gradient(0, height, screenWidth, UI_BORDER, 
		UI_COL(UIC_SHADOW),	UI_COL(UIC_BG_DARK), 1, 0);
	UI_TextOutEx(text, UI_BORDER, height/2, false, true, 
		UI_COL(UIC_TEXT), 1);
}

void UI_DrawTitle(ui_page_t *page)
{
	UI_DrawTitleEx(page->title, UI_TITLE_HGT);
	if(1 || !ui_showmouse)
	{
		char *msg = "(Move with Tab/S-Tab)";
		// If the mouse cursor is not visible, print a short help message.
		UI_TextOutEx(msg, screenWidth - UI_BORDER - FR_TextWidth(msg),
			UI_TITLE_HGT/2, false, true, UI_COL(UIC_TEXT), 0.33f);
	}
}

void UI_Shade
	(int x, int y, int w, int h, int border, ui_color_t *main, 
	 ui_color_t *secondary, float alpha, float bottom_alpha)
{
	float s[2][2] = { { 0, 1 }, { 1, 0 } };
	float t[2][2] = { { 0, 1 }, { 1, 0 } };
	ui_color_t *color;
	int i;
	float *u, *v;
	boolean flip = false;
	float beta = 1;

	if(border < 0)
	{
		//flip = true;
		border = -border;
	}
	border = 0;
	if(bottom_alpha < 0) bottom_alpha = alpha;

	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
	gl.Bind(ui_textures[UITEX_SHADE]);
	gl.Begin(DGL_QUADS);
	for(i = 0; i < 2; i++)
	{
		if(!secondary) continue;
		color = (i == 0? main : secondary);
		u = (i == flip? s[0] : s[1]);
		v = (i == flip? t[0] : t[1]);
		if(i == 1) beta = 0.5f;
		
		UI_ColorA(color, alpha * beta);
		gl.TexCoord2f(u[0], v[0]);	gl.Vertex2f(x + border, y + border);
		gl.TexCoord2f(u[1], v[0]);	gl.Vertex2f(x + w - border, y + border);
		UI_ColorA(color, bottom_alpha * beta);
		gl.TexCoord2f(u[1], v[1]);	gl.Vertex2f(x + w - border, y + h - border);
		gl.TexCoord2f(u[0], v[1]);	gl.Vertex2f(x + border, y + h - border);
	}		
	gl.End();
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
}

void UI_Gradient
	(int x, int y, int w, int h, ui_color_t *top, ui_color_t *bottom, 
	 float top_alpha, float bottom_alpha)
{
	UI_GradientEx(x, y, w, h, 0, top, bottom, top_alpha, bottom_alpha);
}

void UI_GradientEx
	(int x, int y, int w, int h, int border, ui_color_t *top, 
	 ui_color_t *bottom, float top_alpha, float bottom_alpha)
{
	//gl.Disable(DGL_TEXTURING);
/*	gl.Begin(DGL_QUADS);
	UI_ColorA(top, top_alpha);
	gl.Vertex2f(x, y);
	gl.Vertex2f(x+w, y);
	UI_ColorA(bottom? bottom : top, bottom_alpha);
	gl.Vertex2f(x+w, y+h);
	gl.Vertex2f(x, y+h);
	gl.End();*/
	UI_DrawRectEx(x, y, w, h, border, true, top, bottom, top_alpha, 
		bottom_alpha);
	//gl.Enable(DGL_TEXTURING);
}

void UI_HorizGradient
	(int x, int y, int w, int h, ui_color_t *left, ui_color_t *right, 
	 float left_alpha, float right_alpha)
{
	gl.Bind(ui_textures[UITEX_HINT]);
	gl.Begin(DGL_QUADS);
	UI_ColorA(left, left_alpha);
	gl.TexCoord2f(0, 1);
	gl.Vertex2f(x, y + h);
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(x, y);
	UI_ColorA(right? right : left, right_alpha);
	gl.TexCoord2f(1, 0);
	gl.Vertex2f(x + w, y);
	gl.TexCoord2f(1, 1);
	gl.Vertex2f(x + w, y + h);
	gl.End();
}

void UI_Line(int x1, int y1, int x2, int y2, ui_color_t *start, 
			 ui_color_t *end, float start_alpha, float end_alpha)
{
	gl.Disable(DGL_TEXTURING);
	gl.Begin(DGL_LINES);
	UI_ColorA(start, start_alpha);
	gl.Vertex2f(x1, y1);
	UI_ColorA(end? end : start, end_alpha);
	gl.Vertex2f(x2, y2);
	gl.End();
	gl.Enable(DGL_TEXTURING);
}

// Draw white, shadowed text.
void UI_TextOut(char *text, int x, int y)
{
	UI_TextOutEx(text, x, y, false, false, UI_COL(UIC_TEXT), 1);
}

// Draw shadowed text.
void UI_TextOutEx(char *text, int x, int y, int horiz_center, int vert_center,
				   ui_color_t *color, float alpha)
{
	// Center, if requested.
	if(horiz_center) x -= FR_TextWidth(text)/2;
	if(vert_center) y -= FR_TextHeight(text)/2;
	// Shadow.
	UI_ColorA(UI_COL(UIC_SHADOW), .6f*alpha);
	FR_TextOut(text, x + UI_SHADOW_OFFSET, y + UI_SHADOW_OFFSET);
	// Actual text.
	UI_ColorA(color, alpha);
	FR_TextOut(text, x, y);	
}

//===========================================================================
// UI_TextOutWrap
//===========================================================================
int UI_TextOutWrap(char *text, int x, int y, int w, int h)
{
	return UI_TextOutWrapEx(text, x, y, w, h, UI_COL(UIC_TEXT), 1);
}

//===========================================================================
// UI_TextOutWrapEx
//	Draw line-wrapped text inside a box. Returns the Y coordinate of the 
//	last word.
//===========================================================================
int UI_TextOutWrapEx(char *text, int x, int y, int w, int h, 
					  ui_color_t *color, float alpha)
{
	char word[2048], *wp = word;
	int len, tx = x, ty = y;
	byte c;

	UI_ColorA(color, alpha);
	for(; ; text++)
	{
		c = *text;
		// Whitespace?
		if(!c || c == ' ' || c == '\n' || c == '\b' || c == '-')
		{
			if(c == '-') *wp++ = c; // Hyphens should be included in the word.
			// Time to print the word.
			*wp = 0;
			len = FR_TextWidth(word);
			if(tx + len > x + w) // Doesn't fit?
			{
				tx = x;
				ty += ui_fonthgt;
			}
			// Can't print any more? (always print the 1st line)
			if(ty + ui_fonthgt > y + h && ty != y) return ty; 
			FR_TextOut(word, tx, ty);
			tx += len;
			wp = word;
			// React to delimiter.
			switch(c)
			{
			case 0:
				return ty; // All of the text has been printed.
				
			case ' ':
				tx += FR_TextWidth(" ");
				break;

			case '\n':
				tx = x;
				ty += ui_fonthgt;
				break;

			case '\b':	// Break.
				tx = x;
				ty += 3*ui_fonthgt/2;
				break;
			}
		}
		else
		{
			// Append to word buffer.
			*wp++ = c;
		}
	}
}

void UI_DrawRectEx
	(int x, int y, int w, int h, int brd, boolean filled,
	 ui_color_t *top, ui_color_t *bottom, float alpha, float bottom_alpha)
{
	float s[2] = { 0, 1 }, t[2] = { 0, 1 };

	if(brd < 0) 
	{
		brd = -brd;
		s[0] = t[0] = 1;
		s[1] = t[1] = 0;
	}
	if(bottom_alpha < 0) bottom_alpha = alpha;
	if(!bottom) bottom = top;

	// The fill comes first, if there's one.
	if(filled)
	{
		gl.Bind(ui_textures[UITEX_FILL]);
		gl.Begin(DGL_QUADS);
		gl.TexCoord2f(0.5f, 0.5f);
		UI_ColorA(top, alpha);
		gl.Vertex2f(x + brd, y + brd);
		gl.Vertex2f(x + w - brd, y + brd);
		UI_ColorA(bottom, bottom_alpha);
		gl.Vertex2f(x + w - brd, y + h - brd);
		gl.Vertex2f(x + brd, y + h - brd);
	}
	else
	{
		gl.Bind(ui_textures[UITEX_CORNER]);
		gl.Begin(DGL_QUADS);
	}
	if(!filled || brd > 0)
	{
		// Top Left.	
		UI_ColorA(top, alpha);
		gl.TexCoord2f(s[0], t[0]);	gl.Vertex2f(x, y);
		gl.TexCoord2f(0.5f, t[0]);	gl.Vertex2f(x + brd, y);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + brd, y + brd);
		gl.TexCoord2f(s[0], 0.5f);	gl.Vertex2f(x, y + brd);
		// Top.
		gl.TexCoord2f(0.5f, t[0]);	gl.Vertex2f(x + brd, y);
		gl.TexCoord2f(0.5f, t[0]);	gl.Vertex2f(x + w - brd, y);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + w - brd, y + brd);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + brd, y + brd);
		// Top Right.
		gl.TexCoord2f(0.5f, t[0]);	gl.Vertex2f(x + w - brd, y);
		gl.TexCoord2f(s[1], t[0]);	gl.Vertex2f(x + w, y);
		gl.TexCoord2f(s[1], 0.5f);	gl.Vertex2f(x + w, y + brd);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + w - brd, y + brd);
		// Right.
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + w - brd, y + brd);
		gl.TexCoord2f(s[1], 0.5f);	gl.Vertex2f(x + w, y + brd);
		UI_ColorA(bottom, bottom_alpha);
		gl.TexCoord2f(s[1], 0.5f);	gl.Vertex2f(x + w, y + h - brd);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + w - brd, y + h - brd);
		// Bottom Right.
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + w - brd, y + h - brd);
		gl.TexCoord2f(s[1], 0.5f);	gl.Vertex2f(x + w, y + h - brd);
		gl.TexCoord2f(s[1], t[1]);	gl.Vertex2f(x + w, y + h);
		gl.TexCoord2f(0.5f, t[1]);	gl.Vertex2f(x + w - brd, y + h);
		// Bottom.
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + brd, y + h - brd);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + w - brd, y + h - brd);
		gl.TexCoord2f(0.5f, t[1]);	gl.Vertex2f(x + w - brd, y + h);
		gl.TexCoord2f(0.5f, t[1]);	gl.Vertex2f(x + brd, y + h);
		// Bottom Left.
		gl.TexCoord2f(s[0], 0.5f);	gl.Vertex2f(x, y + h - brd);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + brd, y + h - brd);
		gl.TexCoord2f(0.5f, t[1]);	gl.Vertex2f(x + brd, y + h);
		gl.TexCoord2f(s[0], t[1]);	gl.Vertex2f(x, y + h);
		// Left.
		UI_ColorA(top, alpha);
		gl.TexCoord2f(s[0], 0.5f);	gl.Vertex2f(x, y + brd);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + brd, y + brd);
		UI_ColorA(bottom, bottom_alpha);
		gl.TexCoord2f(0.5f, 0.5f);	gl.Vertex2f(x + brd, y + h - brd);
		gl.TexCoord2f(s[0], 0.5f);	gl.Vertex2f(x, y + h - brd);
	}
	gl.End();
}

void UI_DrawRect
	(int x, int y, int w, int h, int brd, ui_color_t *color, float alpha)
{
	UI_DrawRectEx(x, y, w, h, brd, false, color, NULL, alpha, alpha);
}

void UI_DrawTriangle(int x, int y, int radius, ui_color_t *hi, 
					 ui_color_t *med, ui_color_t *low, float alpha)
{
	float xrad = radius*.866f;	// cos(60)
	float yrad = radius/2;		// sin(60)

	gl.Disable(DGL_TEXTURING);
	gl.Begin(DGL_TRIANGLES);
	
	y += radius/4;

	// Upper left triangle.
	UI_ColorA(radius > 0? hi : med, alpha);
	gl.Vertex2f(x, y);
	gl.Vertex2f(x - xrad, y + yrad);
	UI_ColorA(radius > 0? hi : low, alpha);
	gl.Vertex2f(x, y - radius);

	// Upper right triangle.
	UI_ColorA(low, alpha);
	gl.Vertex2f(x, y);
	gl.Vertex2f(x, y - radius);
	UI_ColorA(med, alpha);
	gl.Vertex2f(x + xrad, y + yrad);

	// Bottom triangle.
	if(radius < 0) UI_ColorA(hi, alpha);
	gl.Vertex2f(x, y);
	gl.Vertex2f(x + xrad, y + yrad);
	UI_ColorA(radius > 0? low : med, alpha);
	gl.Vertex2f(x - xrad, y + yrad);

	gl.End();
	gl.Enable(DGL_TEXTURING);
}

//===========================================================================
// UI_DrawHorizTriangle
//	A horizontal triangle, pointing left or right. Positive radius 
//	means left.
//===========================================================================
void UI_DrawHorizTriangle(int x, int y, int radius, ui_color_t *hi, 
						  ui_color_t *med, ui_color_t *low, float alpha)
{
	float yrad = radius*.866f;	// cos(60)
	float xrad = radius/2;		// sin(60)

	gl.Disable(DGL_TEXTURING);
	gl.Begin(DGL_TRIANGLES);
	
	x += radius/4;

	// Upper left triangle.
	UI_ColorA(radius > 0? hi : med, alpha);
	gl.Vertex2f(x, y);
	if(radius < 0) UI_ColorA(low, alpha);
	gl.Vertex2f(x - radius, y);
	gl.Vertex2f(x + xrad, y - yrad);

	// Lower left triangle.
	UI_ColorA(radius > 0? med : hi, alpha);
	gl.Vertex2f(x, y);
	if(radius < 0) UI_ColorA(hi, alpha);
	gl.Vertex2f(x + xrad, y + yrad);
	UI_ColorA(radius > 0? low : med, alpha);
	gl.Vertex2f(x - radius, y);

	// Right triangle.
	UI_ColorA(radius > 0? med : hi, alpha);
	gl.Vertex2f(x, y);
	UI_ColorA(radius > 0? hi : med, alpha);
	gl.Vertex2f(x + xrad, y - yrad);
	UI_ColorA(radius > 0? low : hi, alpha);
	gl.Vertex2f(x + xrad, y + yrad);

	gl.End();
	gl.Enable(DGL_TEXTURING);
}

//===========================================================================
// UI_DefaultButtonBackground
//===========================================================================
void UI_DefaultButtonBackground(ui_color_t *col, boolean down)
{
	UI_MixColors(UI_COL(UIC_TEXT), UI_COL(UIC_SHADOW), col, 
		down? .1f : .5f);
}

//===========================================================================
// UI_DrawButton
//===========================================================================
void UI_DrawButton(int x, int y, int w, int h, int brd, float alpha, 
				   ui_color_t *background, boolean down, boolean disabled,
				   int arrow)
{
	int inside = MIN_OF(w - brd*2, h - brd*2);
	int boff = down? 2 : 0;
	ui_color_t back;

	if(!background)
	{
		// Calculate the default button color.
		UI_DefaultButtonBackground(&back, down);
		background = &back;
	}

	UI_GradientEx(x, y, w, h, brd, background, 0, disabled? .2f : 1, 0);
	UI_Shade(x, y, w, h, UI_BUTTON_BORDER * (down? -1 : 1), 
		UI_COL(UIC_BRD_HI), UI_COL(UIC_BRD_LOW), alpha/3, -1);
	UI_DrawRectEx(x, y, w, h, brd * (down? -1 : 1), false,
		UI_COL(UIC_BRD_HI), NULL, alpha, -1);
	
	switch(arrow)
	{
	case UIBA_UP:
	case UIBA_DOWN:
		UI_DrawTriangle(x + w/2 + boff, y + h/2 + boff, 
			inside/2.75f * (arrow == UIBA_DOWN? -1 : 1), 
			/*UI_COL(UIC_BRD_HI), UI_COL(UIC_BRD_MED), UI_COL(UIC_BRD_LOW),*/
			UI_COL(UIC_TEXT), UI_COL(UIC_TEXT), UI_COL(UIC_TEXT),
			alpha * (disabled? .2f : 1));
		break;

	case UIBA_LEFT:
	case UIBA_RIGHT:
		UI_DrawHorizTriangle(x + w/2 + boff, y + h/2 + boff,
			inside/2.75f * (arrow == UIBA_RIGHT? -1 : 1),
			/*UI_COL(UIC_BRD_HI), UI_COL(UIC_BRD_MED), UI_COL(UIC_BRD_LOW),*/
			UI_COL(UIC_TEXT), UI_COL(UIC_TEXT), UI_COL(UIC_TEXT),
			alpha * (disabled? .2f : 1));
		break;
	}
}

//===========================================================================
// UI_DrawHelpBox
//===========================================================================
void UI_DrawHelpBox(int x, int y, int w, int h, float alpha, char *text)
{
	int bor = UI_BUTTON_BORDER;

	UI_GradientEx(x, y, w, h, bor,
		UI_COL(UIC_HELP), UI_COL(UIC_HELP),	alpha/4, alpha/2);
	UI_DrawRectEx(x, y, w, h, bor, false, UI_COL(UIC_BRD_HI), NULL, 
		alpha, -1);

	if(text)
	{
		bor = 2 * UI_BORDER / 3;
		UI_TextOutWrapEx(text, x + 2*bor, y + 2*bor, w - 4*bor, h - 4*bor, 
			UI_COL(UIC_TEXT), alpha);
	}
}

void UI_DrawMouse(int x, int y)
{
	float scale = MAX_OF(1, screenWidth/640.0f);

	if(!ui_showmouse) return;

	x--; y--;
	gl.Color3f(1, 1, 1);
	gl.Bind(ui_textures[UITEX_MOUSE]);
	gl.Begin(DGL_QUADS);
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(x, y);
	gl.TexCoord2f(1, 0);
	gl.Vertex2f(x + uiMouseWidth * scale, y);
	gl.TexCoord2f(1, 1);
	gl.Vertex2f(x + uiMouseWidth * scale, y + uiMouseHeight * scale);
	gl.TexCoord2f(0, 1);
	gl.Vertex2f(x, y + uiMouseHeight * scale);
	gl.End();
}

//===========================================================================
// UI_DrawLogo
//===========================================================================
void UI_DrawLogo(int x, int y, int w, int h)
{
	gl.Bind(ui_textures[UITEX_LOGO]);
	GL_DrawRect(x, y, w, h, 1, 1, 1, 1);
}

//===========================================================================
// CCmdUIColor
//	Change the UI colors.
//===========================================================================
int CCmdUIColor(int argc, char **argv)
{
	const char *objects[] = // Also a mapping to UIC.
	{
		"text",
		"shadow",
		"bglight",
		"bgmed",
		"bgdark",
		"borhigh",
		"bormed",
		"borlow",
		"help",
		NULL
	};
	int i;

	if(argc != 5)
	{
		Con_Printf("%s (object) (red) (green) (blue)\n", argv[0]);
		Con_Printf("Possible objects are:\n");
		Con_Printf(" text, shadow, bglight, bgmed, bgdark,\n"
			" borhigh, bormed, borlow, help\n");
		Con_Printf("Color values must be in range 0..1.\n");
		return true;
	}
	for(i = 0; objects[i]; i++)
		if(!stricmp(argv[1], objects[i]))
		{
			ui_colors[i].red = strtod(argv[2], 0);
			ui_colors[i].green = strtod(argv[3], 0);
			ui_colors[i].blue = strtod(argv[4], 0);
			return true;
		}

	Con_Printf("Unknown UI object '%s'.\n", argv[1]);
	return false;
}
