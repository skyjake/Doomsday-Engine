<?php
/*
 * Doomsday Web API: JSON Data Dump
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

require_once('include/class.session.php');
require_once('include/builds.inc.php');

define('RECENT_THRESHOLD', 3600*72);

$dl_platforms = [
    'win-x64',
    'win-x86',
    'mac10_10-x86_64',
    'ubuntu18-x86_64',
    'fedora23-x86_64',
    'source'
];

function latest_stable_version()
{
    global $dl_platforms;

    $db = Session::get()->database();
    $vers = [];
    foreach ($dl_platforms as $platform) {        
        $vers += [$platform => db_latest_version($db, $platform, BT_STABLE)];
    }
    return $vers;
}

function sitemap_info()
{
    $db = Session::get()->database();
    
    // Check any recently announced servers.
    $recent_servers = [];
    $result = db_query($db, "SELECT * FROM servers ORDER BY player_count DESC, timestamp DESC LIMIT 4");
    while ($row = $result->fetch_assoc()) {
        $pnum = $row['player_count'];
        $pmax = $row['player_max'];
        $name = htmlentities(urldecode($row['name']));
        $game_id = $row['game_id'];
        
        $recent_servers[] = [
            'pnum' => (int) $pnum,
            'pmax' => (int) $pmax,
            'name' => $name,
            'game_id' => $game_id            
        ];        
    }

    // Contact the BDB for a list of the latest builds.
    $result = db_query($db, "SELECT build, version, type, UNIX_TIMESTAMP(timestamp) FROM "
        .DB_TABLE_BUILDS." ORDER BY timestamp DESC");    
    $new_threshold = time() - RECENT_THRESHOLD;
    $count = 4;
    $build_list = [];
    while ($row = $result->fetch_assoc()) {            
        $version = omit_zeroes($row['version']);
        $ts = (int) $row['UNIX_TIMESTAMP(timestamp)'];
        $date = gmstrftime('%B %e', $ts);
        $build_list[] = [
            'build'     => (int) $row['build'],
            'version'   => $version,
            'type'      => ucwords(build_type_text($row['type'])),
            'is_recent' => ($ts > $new_threshold),
            'date'      => $date
        ];
        
        if (--$count == 0) break;
    }
    
    return [
        'servers' => $recent_servers, 
        'builds'  => $build_list
    ];
}

function dump_download_badge($file_id)
{
    $db = Session::get()->database();

    // Fetch all information about this file.
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_FILES." WHERE id=$file_id");
    $file = $result->fetch_assoc();
    
    $build = db_get_build($db, $file['build']);
    $build_type = ucwords(build_type_text($build['type']));
    if ($build_type == 'Candidate') {
        $build_type = 'RC';
    }
    $version = $build['version'];
    
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_PLATFORMS." WHERE id=$file[plat_id]");
    $plat = $result->fetch_assoc();

    $ext = pathinfo($file['name'], PATHINFO_EXTENSION);
    // Special case for making a distinction between old .dmg files.
    if ($ext == 'dmg' && strpos($file['name'], '_apps') != FALSE) {
        $fext = 'Applications';
    }
    else if ($plat['os'] == 'macx') {
        $fext = $ext;
    }
    else {
        $fext = "$plat[cpu_bits]-bit ".$ext;
    }

    /*$title = "Doomsday ".omit_zeroes($version);
    if ($build['type'] != BT_STABLE) {
        $title = omit_zeroes($version).' '
            .$build_type." #$build[build]";
    }
    if ($plat['platform'] == 'source') {
        $title .= " (Source)";
    }
    else {
        $title .= " &mdash; $fext";        
    }*/
    //$full_title = "Doomsday ".human_version($version, $build['build'], build_type_text($build['type']))
    //    ." for ".$plat['name']." (".$plat['cpu_bits']."-bit)";
    //    $download_url = DENG_API_URL.'/builds?dl='.$file['name'];
    
    return [
        'file_id' => (int) $file_id,
        'os' => $plat['os'],
        'cpu_bits' => (int) $plat['cpu_bits'],
        'platform' => $plat['platform'],
        'platform_name' => $plat['name'],
        'build' => (int) $build['build'],
        'type' => (int) $build['type'],
        'build_type' => build_type_text($build['type']),
        'version' => $version,
        'timestamp' => $build['timestamp'],
        'filename' => $file['name'],
        'fext' => $fext        
    ];
    
    /*
    $metadata = '';
    if ($plat['os'] != 'any') {
        $metadata .= $plat['name'].' (or later)';
    }
    else {
        $metadata .= "Source code .tar.gz";
    }
    $metadata .= "<time datetime='$build[timestamp]'> &middot; "
        .reformat_date($build['timestamp']).'</time> '; 
    
    echo('<p><div class="package-badge">'
        ."<a class='package-dl' href='$download_url' "
        ."title=\"Download $full_title\"><div class='package-name'>$title</div><div class='package-metadata'>"
        .$metadata
        ."</div></a></div></p>\n");
    */
}

function dump_download_badges($platform, $type)
{
    // Find the latest suitable files.
    $db = Session::get()->database();
    $result = db_latest_files($db, $platform, $type);
    $latest_build = 0;
    $badges = [];
    while ($row = $result->fetch_assoc()) {        
        // All suitable files of the latest build will be shown.
        if ($latest_build == 0) {
            $latest_build = $row['build'];
        }
        else if ($latest_build != $row['build']) {
            break;
        }
        $badges[] = dump_download_badge($row['id']);
    }
    return $badges;
}

function download_info()
{
    global $dl_platforms;
    
    $db = Session::get()->database();
    $dl_types = [   
        BT_STABLE,
        BT_CANDIDATE,
        BT_UNSTABLE
    ];
    
    $got_rc = [];
    $badges = [];
    for ($i = 0; $i < count($dl_platforms); $i++) {
        $plat = $dl_platforms[$i];
        
        $got_rc += [ $plat => is_release_candidate_available($plat) ];
        
        $plat_badges = [];
        foreach ($dl_types as $btype) {
            $listed = dump_download_badges($plat, $btype);
            if (count($listed) > 0) {
                $type = build_type_text($btype);
                $plat_badges += [ $type => $listed ];
            }
        }
        $badges += [ $plat => $plat_badges ];
    }
    return [
        'got_rc' => $got_rc,
        'badges' => $badges
    ];
}

function generate_dump_json()
{
    header('Content-Type: application/json');
    $data = array_merge(
        [ 'latest_stable_version' => latest_stable_version() ],
        [ 'sitemap_info' => sitemap_info() ],
        [ 'downloads' => download_info() ]
    );
    echo(json_encode($data)."\n");
}

//---------------------------------------------------------------------------------------

setlocale(LC_ALL, 'en_US.UTF-8');

if ($_SERVER['REQUEST_METHOD'] == 'GET') {
    generate_dump_json();
}
