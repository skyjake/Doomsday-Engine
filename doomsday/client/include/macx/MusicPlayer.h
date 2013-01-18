/** @file
 *
 * @authors Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>

@interface MusicPlayer : NSObject
{
    QTMovie* currentSong;
    float songVolume;
}

- (id)init;
- (void)shutdown;
- (int)playFile:(const char*)filename looping:(int)loop;
- (void)setVolume:(float)volume;
- (void)play;
- (void)stop;
- (void)rewind;

@end

