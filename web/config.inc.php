<?php
/**
 * @file config.inc.php
 * Top-level configuration file for the homepage.
 *
 * @section License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
 */

// Framework configuration:
$siteconfig['VisibleErrors'] = false;

// Site configuration:
$siteconfig['HomeAddress'] = 'http://dengine.net';
$siteconfig['Title'] = 'Doomsday Engine';
$siteconfig['Author'] = 'David Gardner';
$siteconfig['AuthorEmail'] = 'eunbolt@gmail.com';
$siteconfig['Designer'] = 'Daniel Swanson';
$siteconfig['DesignerEmail'] = 'danij@dengine.net';
$siteconfig['Copyright'] = 'Copyright (c) ' . date('Y') .', Deng Team';
$siteconfig['Description'] = 'Home of the Doomsday Engine, a modern engine for playing classic 2.5d first person shooters DOOM, Heretic and Hexen.';
$siteconfig['RobotRevisitDays'] = 3;
$siteconfig['Keywords'] = array(
    'Doomsday', 'Doomsday Engine', 'Deng',
    'Doom', 'Heretic', 'Hexen',
    'jDoom',  'jHeretic', 'jHexen',
    'sourceport', 'first person shooter', 'fps');
