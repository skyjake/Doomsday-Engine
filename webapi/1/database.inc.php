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

function db_list_build_files($db, $build)
{
    $result = db_query($db, "SELECT id FROM ".DB_TABLE_FILES." WHERE build=$build");
    $files = array();
    while ($row = $result->fetch_assoc()) {
        $files[] = $row['id'];
    }
    return $result;
}

function db_file_path($db, $file_id)
{
    $result = db_query($db, "SELECT name FROM ".DB_TABLE_FILES." WHERE id=$file_id");
    if ($row = $result->fetch_assoc()) {
        return DENG_FILE_ARCHIVE_PATH.'/'.$row['name'];
    }
    return '';
}

function db_build_binary_count($db, $build)
{
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_FILES
        ." WHERE build=$build AND type=".FT_BINARY);
    return $result->num_rows;
}

function db_build_summary($db, $build)
{
    // Fetch build info.
    $result = db_query($db, "SELECT UNIX_TIMESTAMP(timestamp), blurb, changes"
        ." FROM ".DB_TABLE_BUILDS." WHERE build=$build");
    $row = $result->fetch_assoc();
    $date = gmstrftime(RFC_TIME, $row['UNIX_TIMESTAMP(timestamp)']);
    $bin_count = db_build_binary_count($db, $build);
    
    $text = "<p>The autobuilder started build $build on $date"
        ." and produced $bin_count package"
        .($bin_count != 1? 's.' : '.');
    
    // Summary change tags.
    if (($changes = json_decode($row['changes'])) != NULL) {
        // Total commit count.
        $commit_count = count($changes->commits);
        if ($commit_count > 0) {
            $text .= " The build contains $commit_count commit";
            if ($commit_count != 1) $text .= 's';   
            $tags = array();
            // Collect commit tags.     
            foreach ($changes->commits as $commit) {
                foreach ($commit->tags as $tag) {
                    if (in_array($tag, $tags)) {
                        $tags[$tag] += 1;
                    }
                    else {
                        $tags[$tag] = 1;
                    }
                }
            }
            asort($tags);
            array_reverse($tags);
            $common_tags = array_slice(array_keys($tags), 0, 5);
            $common_count = count($common_tags);
            $text .= ", and the most used tag";     
            if ($common_count == 1) {
                $text .= " is ".$common_tags[0];
            }
            else {
                $text .= "s are "
                    .join(', ', array_slice($common_tags, 0, $common_count - 1))
                    .' and '.$common_tags[$common_count - 1];
            }
            $text .= '.';
        }
    }
    $text .= '</p>';
    if (!empty($row['blurb'])) {
        $text .= $row['blurb'];
    }
    return $text;
}

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
