<?php
/*
 * Doomsday Web API: Build Database Manipulation
 * Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

require_once('database.inc.php');

function detect_user_platform()
{
    require_once(__DIR__.'/../lib/Browser.php');
    
    // Check what the browser tells us.
    $browser = new Browser();    
    switch ($browser->getPlatform()) {
        case Browser::PLATFORM_WINDOWS:
            $user_platform = 'windows';
            break;
        case Browser::PLATFORM_APPLE:
            $user_platform = 'macx';
            break;
        case Browser::PLATFORM_LINUX:
            $user_platform = 'linux';
            break;
        case Browser::PLATFORM_FREEBSD:
        case Browser::PLATFORM_OPENBSD:
        case Browser::PLATFORM_NETBSD:
            $user_platform = 'any';
            break;
        default:
            $user_platform = '';
            break;
    }
    return $user_platform;
}

function omit_zeroes($version)
{
    $parts = split("\\.", $version);
    while (count($parts) > 2 && array_slice($parts, -1)[0] == '0') {
        $parts = array_slice($parts, 0, -1);
    }
    return join('.', $parts);
}

function human_version($version, $build, $release_type)
{    
    return omit_zeroes($version)." ".ucwords($release_type)." [#${build}]";
}

function basic_markdown($text)
{
    $text = str_replace("\n\n", "<p>", $text);
    $text = preg_replace("/IssueID #([0-9]+)/", 
        "<a href='".DENG_TRACKER_URL."/issues/$1'>IssueID #$1</a>", $text);
        $text = preg_replace("/Fixes #([0-9]+)/", 
            "<a href='".DENG_TRACKER_URL."/issues/$1'>Fixes #$1</a>", $text);
    $text = preg_replace("/`([^\\n]+)`/", "<tt>$1</tt>", $text);
    return $text;
}

function join_list($values)
{
    if (count($values) > 1) {
        return join(', ', array_slice($values, 0, -1)).' and '
            .array_slice($values, -1)[0];
    }
    else if (count($values) == 1) {
        return $values[0];
    }        
    return '';
}

function db_platform_list($db)
{
    $platforms = [];
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_PLATFORMS." ORDER BY ord");
    while ($row = $result->fetch_assoc()) {
        $platforms[] = $row;
    }
    return $platforms;
}

function db_file_path($db, $file_id)
{
    $result = db_query($db, "SELECT name FROM ".DB_TABLE_FILES." WHERE id=$file_id");
    if ($row = $result->fetch_assoc()) {
        return DENG_FILE_ARCHIVE_PATH.'/'.$row['name'];
    }
    return '';
}

function cmp_name($a, $b) 
{
    return strcmp($a['name'], $b['name']); 
}

function download_link($name)
{
    return DENG_API_URL."/builds?dl=".$name;
}

function sfnet_link($build_type, $version, $name)
{
    $sfnet_url = 'http://sourceforge.net/projects/deng/files/Doomsday%20Engine/';
    if ($build_type == BT_STABLE) {
        $sfnet_url .= "$version/";
    }
    else {
        $sfnet_url .= "Builds/";
    }
    return $sfnet_url . "$name/download";
}

function db_build_list_platform_files($db, $build, $platform, $file_type)
{
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_FILES
        ." WHERE build=$build AND plat_id=$platform AND type=$file_type");
    $files = [];
    while ($row = $result->fetch_assoc()) {
        $files[] = $row;
    }
    usort($files, "cmp_name");    
    return $files;
}

function db_build_list_files($db, $build)
{
    $result = db_query($db, "SELECT id FROM ".DB_TABLE_FILES." WHERE build=$build");
    $files = [];
    while ($row = $result->fetch_assoc()) {
        $files[] = $row['id'];
    }
    return $files;
}

function db_build_binary_count($db, $build)
{
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_FILES
        ." WHERE build=$build AND type=".FT_BINARY);
    return $result->num_rows;
}

function db_build_count($db, $type)
{
    $result = db_query($db, "SELECT build FROM ".DB_TABLE_BUILDS
        ." WHERE type=".$type);
    return $result->num_rows;
}

function tag_cmp($a, $b) { 
    if ($b[1] == $a[1]) {
        return strcasecmp($a[0], $b[0]);
    }
    return $b[1] - $a[1]; 
}

function db_build_plaintext_summary($db, $build)
{
    // Fetch build info.
    $result = db_query($db, "SELECT UNIX_TIMESTAMP(timestamp), type, version, blurb, changes"
        ." FROM ".DB_TABLE_BUILDS." WHERE build=$build");
    $row = $result->fetch_assoc();
    $type = build_type_text($row['type']);
    $version = omit_zeroes($row['version']);
    $date = gmstrftime(RFC_TIME, $row['UNIX_TIMESTAMP(timestamp)']);
    $bin_count = db_build_binary_count($db, $build);
    
    $text = "The autobuilder started build $build of $type $version on $date"
        ." and produced $bin_count package"
        .($bin_count != 1? 's.' : '.');
    
    // Summarize change tags.
    if (($changes = json_decode($row['changes'])) != NULL) {
        // Total commit count.
        $commit_count = count($changes->commits);
        if ($commit_count > 0) {
            $text .= " The build contains $commit_count commit";
            if ($commit_count != 1) $text .= 's';   
            $tags = [];
            // Collect commit tags.     
            foreach ($changes->commits as $commit) {
                foreach ($commit->tags as $tag) {
                    if (array_key_exists($tag, $tags)) {
                        $tags[$tag] += 1;
                    }
                    else {
                        $tags += [$tag => 1];
                    }
                }
            }
            // Sort tags by count, descending.
            $otags = [];
            foreach ($tags as $tag => $count) {
                $otags[] = array($tag, $count);
            }
            usort($otags, "tag_cmp");
            $tags = [];
            foreach ($otags as $tag) {
                $tags[] = $tag[0];
            }
            // List up to 5 tags.
            $common_tags = array_slice($tags, 0, 5);
            $common_count = count($common_tags);
            if ($common_count > 0) {
                $text .= ", and the most used tag";     
                if ($common_count == 1) {
                    $text .= " is ".$common_tags[0];
                }
                else {
                    $text .= "s are ".join_list($common_tags);
                }
            }
            $text .= '.';
        }
    }
    return $text;
}

function db_build_summary($db, $build)
{
    // Fetch build info.
    $result = db_query($db, "SELECT UNIX_TIMESTAMP(timestamp), type, version, blurb"
        ." FROM ".DB_TABLE_BUILDS." WHERE build=$build");
    $row = $result->fetch_assoc();
    $type = build_type_text($row['type']);
    $version = omit_zeroes($row['version']);
    
    $text = '<p>'.db_build_plaintext_summary($db, $build).'</p>';
    if (!empty($row['blurb'])) {
        $text .= $row['blurb'];
    }
    if ($type == 'stable') {
        $ver = omit_zeroes($version);
        $relnotes_link = DENG_WIKI_URL."/Doomsday_version_".$ver;
        $text .= "<p>See also: <a href='$relnotes_link'>Release notes for $ver</a></p>";
    }
    return $text;
}

function collect_tags($commits)
{
    // These words are always considered to be valid tags.
    $tags = ['Cleanup'=>true, 
             'Fixed'=>true, 
             'Added'=>true, 
             'Refactor'=>true, 
             'Performance'=>true, 
             'Optimize'=>true, 
             'Merge branch'=>true];
    foreach ($commits as $commit) {
        foreach ($commit->tags + $commit->guessedTags as $tag) {
            if (!array_key_exists($tag, $tags)) {
                $tags += [$tag=>true];
            }
        }
    }
    return array_keys($tags);
}

function group_commits_by_tag($commits)
{
    $remaining_commits = [];
    foreach ($commits as $commit) {
        $remaining_commits += [$commit->hash => $commit];
    }
    
    $groups = [];
    foreach (collect_tags($commits) as $tag) {
        $groups += [$tag => []];
        foreach ($remaining_commits as $commit) {
            if (in_array($tag, $commit->tags)) {
                $groups[$tag][] = $commit;
            }
        }
        foreach ($remaining_commits as $commit) {
            if (in_array($tag, $commit->guessedTags)) {
                $groups[$tag][] = $commit;
            }
        }
        foreach ($groups[$tag] as $commit) {
            unset($remaining_commits[$commit->hash]);
        }
    }
    
    // Anything still left is put in the Misc group.
    $groups += ['Miscellaneous' => []];
    foreach ($remaining_commits as $commit) {
        $groups['Miscellaneous'][] = $commit;
    }
    
    return $groups;    
}

function db_get_build($db, $build)
{
    return db_query($db, "SELECT * FROM ".DB_TABLE_BUILDS
        ." WHERE build=$build")->fetch_assoc();
}

function db_latest_files($db, $platform, $build_type, $limit=NULL)
{
    $plat = db_get_platform($db, $platform);
    if ($build_type == BT_CANDIDATE) {
        $type_cond = "b.type!=".BT_UNSTABLE; // can also be stable
    }
    else {
        $type_cond = "b.type=".$build_type;
    }
    $limiter = (isset($limit)? "LIMIT $limit" : '');
    $plat_id = $plat['id'];
    return db_query($db, "SELECT f.id, f.name, f.size, f.md5, f.build, b.version, b.major, b.minor, b.patch, UNIX_TIMESTAMP(b.timestamp) FROM ".DB_TABLE_FILES
        ." f LEFT JOIN ".DB_TABLE_BUILDS." b ON f.build=b.build "
        ."WHERE $type_cond AND f.plat_id=$plat_id ORDER BY b.timestamp DESC, f.name "
        .$limiter);    
}

function generate_platform_latest_json($platform, $build_type_txt)
{
    header('Content-Type: application/json');

    $db = db_open();
    $plat = db_get_platform($db, $platform);
    if (empty($plat)) {
        echo("{}\n");
        $db->close();
        return;
    }
    $type = build_type_from_text($build_type_txt);
    $result = db_latest_files($db, $platform, $type, 1);
    /*if ($type == BT_CANDIDATE) {
        $type_cond = "b.type!=".BT_UNSTABLE; // can also be stable
    }
    else {
        $type_cond = "b.type=".$type;
    }
    $result = db_query($db, "SELECT f.name, f.size, f.md5, f.build, b.version, b.major, b.minor, b.patch, UNIX_TIMESTAMP(b.timestamp) FROM ".DB_TABLE_FILES
        ." f LEFT JOIN ".DB_TABLE_BUILDS." b ON f.build=b.build "
        ."WHERE $type_cond AND f.plat_id=$plat[id] ORDER BY b.timestamp DESC, f.name "
        ."LIMIT 1");*/
    $resp = [];        
    if ($row = $result->fetch_assoc()) {
        $filename = $row['name'];
        $build = $row['build'];
        $version = human_version($row['version'], $build, build_type_text($type));
        $plat_name = $plat['name'];
        $bits = $plat['cpu_bits'];
        $date = gmstrftime(RFC_TIME, $row['UNIX_TIMESTAMP(b.timestamp)']);
        if ($bits > 0) {
            $full_title = "Doomsday $version for $plat_name or later (${bits}-bit)";
        }
        else {
            $full_title = "Doomsday $version $plat_name";
        }
        $resp += [
            'build_uniqueid' => (int) $build,
            'build_type' => build_type_text($type),
            'build_startdate' => $date,
            'platform_name' => $platform,
            'title' => "Doomsday ".omit_zeroes($row['version']),
            'fulltitle' => $full_title,
            'version' => $row['version'],
            'version_major' => (int) $row['major'],           
            'version_minor' => (int) $row['minor'],           
            'version_patch' => (int) $row['patch'],
            'direct_download_uri' => download_link($filename),
            'direct_download_fallback_uri' => download_link($filename)."&mirror=sf",
            'file_size' => (int) $row['size'],
            'file_md5' => $row['md5'],
            'release_changeloguri' => DENG_API_URL."/builds?number=$build&format=html",
            'release_notesuri' => DENG_WIKI_URL."/Doomsday_version_"
                .omit_zeroes($row['version']),
            'release_date' => $date,
            'is_unstable' => ($type == BT_UNSTABLE)
        ];
        echo(json_encode($resp));    
    }
    else {
        echo("{}\n");
    }
    $db->close();
}
