/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * m_args.h: Command Line Arguments
 */

#ifndef __COMMAND_LINE_ARGS_H__
#define __COMMAND_LINE_ARGS_H__

void		ArgInit(const char *cmdline);
void		ArgShutdown(void);
void		ArgAbbreviate(char *longname, char *shortname);

int			Argc(void);
char *		Argv(int i);
char **		ArgvPtr(int i);
char *		ArgNext(void);
int			ArgCheck(char *check);
int			ArgCheckWith(char *check, int num);
int			ArgExists(char *check);
int			ArgIsOption(int i);
int			ArgRecognize(char *first, char *second);

#endif //__COMMAND_LINE_ARGS_H__
