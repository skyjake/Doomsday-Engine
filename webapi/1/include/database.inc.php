<?php
/*
 * Doomsday Web API: Database Access
 * Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * License: GPL v2+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses/gpl.html
 */

require_once('api_config.inc.php'); // database config

// Opens the database connection.
// @return MySQLi object.
function db_open()
{
    return new mysqli(DENG_DB_HOST, DENG_DB_USER, DENG_DB_PASSWORD, DENG_DB_NAME);
}

function db_query(&$db, $sql)
{
    $result = $db->query($sql);
    if (!$result) {
        echo "Database error: " . $db->error;
        echo "Query was: " . $sql;
        exit;
    }
    return $result;
}

//---------------------------------------------------------------------------------------

define('DB_TABLE_BUILDS',    'bdb_builds');
define('DB_TABLE_FILES',     'bdb_files');
define('DB_TABLE_PLATFORMS', 'bdb_platforms');

define('FT_NONE',      0);
define('FT_BINARY',    1);
define('FT_LOG',       2);

define('BT_UNSTABLE',  0);
define('BT_CANDIDATE', 1);
define('BT_STABLE',    2);

function build_type_text($build_type)
{
    switch ($build_type) {
        case BT_UNSTABLE:
            return 'unstable';
        case BT_CANDIDATE:
            return 'candidate';
        case BT_STABLE:
            return 'stable';
    }
    return '';
}

function build_type_from_text($text)
{
    return ($text == 'stable'? BT_STABLE : ($text == 'candidate'? BT_CANDIDATE : BT_UNSTABLE));    
}

function db_get_platform($db, $platform)
{
    $platform = $db->real_escape_string($platform);
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_PLATFORMS
        ." WHERE platform='$platform'");
    return $result->fetch_assoc();
}

function db_find_platform_id($db, $platform)
{
    $plat = db_get_platform($db, $platform);
    if (array_key_exists('id', $plat)) {
        return $plat['id'];
    }
    return 0;
}
