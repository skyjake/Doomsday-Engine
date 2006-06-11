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
 * config.cpp: Configuration Dialog 
 */

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"
#include "resource.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int		wantedAdapter = 0;
int		wantedColorDepth = 0;
int		wantedTexDepth = 0;
int		wantedZDepth = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// ConfigDialogProc
//===========================================================================
BOOL CALLBACK ConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, 
							   LPARAM lParam)
{
	int numAdapters = d3d->GetAdapterCount();
	D3DADAPTER_IDENTIFIER8 adapterId;
	int i;
	HWND it;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		// Init the list boxes.
		it = GetDlgItem(hwndDlg, IDC_DRIVER_LIST);
		for(i = 0; i < numAdapters; i++)
		{
			d3d->GetAdapterIdentifier(i, D3DENUM_NO_WHQL_LEVEL, &adapterId);
			SendMessage(it, LB_ADDSTRING, 0, (LPARAM) adapterId.Description);
			if(wantedAdapter == i)
				SendMessage(it, LB_SETCURSEL, (WPARAM) i, 0);
		}

		it = GetDlgItem(hwndDlg, IDC_COLOR_LIST);
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "Auto");
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "16-bit");
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "32-bit");
		SendMessage(it, LB_SETCURSEL, (WPARAM) wantedColorDepth == 16? 1 
			: wantedColorDepth == 32? 2 : 0, 0);

		it = GetDlgItem(hwndDlg, IDC_TEXTURE_LIST);
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "Auto");
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "16-bit");
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "32-bit");
		SendMessage(it, LB_SETCURSEL, (WPARAM) wantedTexDepth == 16? 1 
			: wantedTexDepth == 32? 2 : 0, 0);
		
		it = GetDlgItem(hwndDlg, IDC_ZBUFFER_LIST);
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "Auto");
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "16-bit");
		SendMessage(it, LB_ADDSTRING, 0, (LPARAM) "32-bit");
		SendMessage(it, LB_SETCURSEL, (WPARAM) wantedZDepth == 16? 1 
			: wantedZDepth == 32? 2 : 0, 0);

		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			// Store the settings.
			i = SendMessage(GetDlgItem(hwndDlg, IDC_DRIVER_LIST), 
				LB_GETCURSEL, 0, 0);
			if(i >= 0) wantedAdapter = i;
			
			i = SendMessage(GetDlgItem(hwndDlg, IDC_COLOR_LIST),
				LB_GETCURSEL, 0, 0);
			wantedColorDepth = i==2? 32 : i==1? 16 : 0;

			i = SendMessage(GetDlgItem(hwndDlg, IDC_TEXTURE_LIST),
				LB_GETCURSEL, 0, 0);
			wantedTexDepth = i==2? 32 : i==1? 16 : 0;

			i = SendMessage(GetDlgItem(hwndDlg, IDC_ZBUFFER_LIST),
				LB_GETCURSEL, 0, 0);
			wantedZDepth = i==2? 32 : i==1? 16 : 0;

			EndDialog(hwndDlg, TRUE);
			return TRUE;

		case IDCANCEL:
			EndDialog(hwndDlg, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}
 
//===========================================================================
// ConfigDialog
//===========================================================================
int ConfigDialog(void)
{	
	int ret;

	ShowCursor(TRUE);
	ShowCursor(TRUE);
	ret = DialogBox(hinst, MAKEINTRESOURCE(IDD_D3DCONFIG), window->hwnd, 
		ConfigDialogProc);
	ShowCursor(FALSE);
	ShowCursor(FALSE);
	return ret;
}

//===========================================================================
// ReadConfig
//===========================================================================
void ReadConfig(void)
{
	static int first_time = DGL_TRUE;
	int showconf = DGL_FALSE;

	DP("ReadConfig:");

	// The config INI is only read once.
	if(!first_time) 
	{
		DP("  Not the first time; not reading config.");
		return;
	}
	first_time = DGL_FALSE;

	wantedAdapter = GetPrivateProfileInt("drD3D", "Adapter", 0, "drD3D.ini");
	wantedColorDepth = GetPrivateProfileInt("drD3D", "Color", 0, "drD3D.ini");
	wantedTexDepth = GetPrivateProfileInt("drD3D", "Texture", 0, "drD3D.ini");
	wantedZDepth = GetPrivateProfileInt("drD3D", "ZBuffer", 0, "drD3D.ini");
	showconf = GetPrivateProfileInt("drD3D", "CfgNextTime", 1, "drD3D.ini");

	if(showconf 
		|| (GetAsyncKeyState(VK_SHIFT)
			| GetAsyncKeyState(VK_CONTROL)
			| GetAsyncKeyState(VK_MENU)) & 0x8000
		|| ArgExists("-d3dcfg"))
	{
		// Automagically disable config dialog for the next time.
		WritePrivateProfileString("drD3D", "CfgNextTime", "0", "drD3D.ini");

		if(ConfigDialog())
		{
			// Save the settings.
			char buf[80];
			sprintf(buf, "%i", wantedAdapter);
			WritePrivateProfileString("drD3D", "Adapter", buf, "drD3D.ini");
			sprintf(buf, "%i", wantedColorDepth);
			WritePrivateProfileString("drD3D", "Color", buf, "drD3D.ini");
			sprintf(buf, "%i", wantedTexDepth);
			WritePrivateProfileString("drD3D", "Texture", buf, "drD3D.ini");
			sprintf(buf, "%i", wantedZDepth);
			WritePrivateProfileString("drD3D", "ZBuffer", buf, "drD3D.ini");
		}		
	}

	DP("  wanted: adap=%i, col=%i, tex=%i, z=%i", wantedAdapter,
		wantedColorDepth, wantedTexDepth, wantedZDepth);
	DP("  showconf=%i", showconf);
}

