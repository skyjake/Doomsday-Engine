#!/bin/bash
################################################################################
#  Copyright and License Summary
#  License: GPL
#  Online License Link: http://www.gnu.org/licenses/gpl.html
#  
#  Copyright © 2006-2008 Jamie Jones <yagisan@dengine.net>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, 
#  Boston, MA  02110-1301  USA
################################################################################
#  
#  This script is used to do a find all "Defines" in The Doomsday Engine sources.
#  
################################################################################
FILES_PROCESSED=0
TOP_LEVEL_DIR=$PWD


findprojectfiles()
{
find -name *.c | grep engine > $TOP_LEVEL_DIR/filelist.txt
find -name *.c | grep plugins >> $TOP_LEVEL_DIR/filelist.txt
find -name *.cpp | grep engine  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.cpp | grep plugins  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.h | grep engine  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.h | grep plugins  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.m | grep engine  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.m | grep plugins  >> $TOP_LEVEL_DIR/filelist.txt

FILE_LIST=`cat $TOP_LEVEL_DIR/filelist.txt`
}

scanfiles()
{
let FILES_PROCESSED=0

for CURRENT_FILE in $FILE_LIST ;
do
#	echo $CURRENT_FILE
	grep  '#if' $CURRENT_FILE 
	grep  '#define' $CURRENT_FILE  
	grep  '#elif' $CURRENT_FILE 
	grep  '#else' $CURRENT_FILE 

	let FILES_PROCESSED=FILES_PROCESSED+1
done
}

if [[ -e $TOP_LEVEL_DIR/doxygen ]]
then
	findprojectfiles
	scanfiles
fi

rm $TOP_LEVEL_DIR/filelist.txt


