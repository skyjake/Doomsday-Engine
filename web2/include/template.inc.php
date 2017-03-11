<?php

require_once('class.session.php');
require_once(DENG_API_DIR.'/include/builds.inc.php');
    
function generate_page_header($title = NULL)
{
    if ($title) {
        $title = " | ".$title;    
    }
    echo("<!DOCTYPE html>\n"
        ."<html lang='en'>\n"
        ."  <head>\n"
        ."  <meta charset='UTF-8'>\n"
        ."  <link href='".SITE_ROOT."/theme/stylesheets/site.css' rel='stylesheet' type='text/css'>\n"
        ."  <title>Doomsday Engine$title</title>\n"
        ."</head>\n");    
}

function generate_page_title($title)
{
    echo("<div id='page-title'>
        <h1>$title</h1>
    </div>");
}

function platform_download_link()
{
    switch (detect_user_platform()) {
        case 'windows': 
            $dl_link = SITE_ROOT.'/windows'; break;
        case 'macx':
            $dl_link = SITE_ROOT.'/macos'; break;
        case 'linux':
            $dl_link = SITE_ROOT.'/linux'; break;
        default:
            $dl_link = SITE_ROOT.'/source'; break;
    }
    return $dl_link;
}

function generate_download_button()
{
    // Check the latest build.
    $user_platform = detect_user_platform();
    $ckey = cache_key('home', ['dl_button', $user_platform]);
    if (!cache_try_load($ckey)) {
        $dl_link = platform_download_link();
        // Find out the latest stable build.
        $db = Session::get()->database();
        $result = db_query($db, "SELECT version FROM ".DB_TABLE_BUILDS
            ." WHERE type=".BT_STABLE." ORDER BY timestamp DESC LIMIT 1");
        if ($row = $result->fetch_assoc()) {
            $button_label = omit_zeroes($row['version']);
        }
        else {
            $button_label = 'Download';            
        }    
        cache_echo("<a href='$dl_link' class='button' "
            ."title='Download latest stable release'>$button_label <span "
            ."class='downarrow'>&#x21E3;</span></a>");
        cache_store($ckey);
    }            
    cache_dump();    
}

function generate_download_badge($db, $file_id)
{
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
        $fext = $ext." ($plat[cpu_bits]-bit)";
    }

    $title = "Doomsday ".omit_zeroes($version);
    if ($build['type'] != BT_STABLE) {
        $title = omit_zeroes($version).' '
            .$build_type." #$build[build]";
    }
    if ($plat['platform'] == 'source') {
        $title .= " (Source)";
    }
    else {
        $title .= " &ndash; $fext";        
    }
    $full_title = "Doomsday ".human_version($version, $build['build'], build_type_text($build['type']))
        ." for ".$plat['name']." (".$plat['cpu_bits']."-bit)";
    $download_url = 'http://api.dengine.net/1/builds?dl='.$file['name'];
    
    $metadata = '<span class="metalink">';
    $metadata .= '<span title="Release Date">'
        .substr($build['timestamp'], 0, 10)
        .'</span> &middot; '; 
    if ($plat['os'] != 'any') {
        $metadata .= $plat['cpu_bits'].'-bit '.$plat['name'].' (or later)';
    }
    else {
        $metadata .= "Source code .tar.gz";
    }
    $metadata .= '</span>';
    
    echo('<p><div class="package_badge">'
        ."<a class='package_name' href='$download_url' "
        ."title=\"Download $full_title\">$title</a><br />"
        .$metadata
        ."</div></p>\n");
}

function generate_badges($platform, $type)
{
    // Find the latest suitable files.
    $db = Session::get()->database();
    $result = db_latest_files($db, $platform, $type);
    $latest_build = 0;
    while ($row = $result->fetch_assoc()) {        
        // All suitable files of the latest build will be shown.
        if ($latest_build == 0) {
            $latest_build = $row['build'];
        }
        else if ($latest_build != $row['build']) {
            break;
        }
        generate_download_badge($db, $row['id']);
    }
}

function generate_sidebar()
{
    echo(
    "<div id='sidebar'>
        <div class='heading'><a href='".SITE_ROOT."'>About Doomsday</a></div>
        <div class='heading'>Downloads</div>
        <ul>
            <li><a href='windows'>Windows</a></li>
            <li><a href='macos'>macOS</a></li>
            <li><a href='linux'>Linux</a></li>
            <li><a href='source'>Source</a></li>
            <li><a href='/builds'>Recent builds</a></li>
        </ul>
        <div class='heading'><a href='/manual'>User Manual</a></div>
        <ul>
            <li><a href='/manual/getting_started'>Getting started</a></li>
            <li><a href='/manual/multiplayer'>Multiplayer</a></li>
        </ul>
        <div class='heading'>Community</div>
        <ul>
            <li><a href='/talk'>Forums</a></li>
            <li><a href='/support'>Tech support</a></li>
            <li><a href='http://facebook.com/doomsday.engine'>Facebook page</a></li>
            <li><a href='/manual/other_ports'>Other ports</a></li>            
        </ul>
        <div class='heading'>Development</div>
        <ul>
            <li><a href='https://tracker.dengine.net/projects/deng'>Bug Tracker</a></li>
            <li><a href='https://tracker.dengine.net/projects/deng/roadmap'>Roadmap</a></li>
            <li><a href='https://github.com/skyjake/Doomsday-Engine.git'>GitHub</a></li>
            <li><a href='/blog/'>Blog</a></li>
            <li><a href='http://twitter.com/@dengteam'>@dengteam</a></li>
        </ul>
        <div class='heading'><a href='http://sourceforge.net/p/deng/donate/?source=navbar' title='Donate to support the Doomsday Engine Project'>Donate &#9825;</a></div>
    </div>");
}

function generate_sitemap()
{
    echo("<div id='site-map'>
        <ul class='map-wrapper'>
            <li>
                <div>Latest News</div>
                <div>Latest Builds</div>
            </li>
            <li>
                Multiplayer Games
            </li>
            <li>
                User Manual
            </li>
            <li>
                <div>Go to...</div>
                <div>Alternatives</div>
            </li>
        </ul>
        <div id='credits'>
            Doomsday Engine is <a href='https://github.com/skyjake/Doomsday-Engine.git'>open 
            source software</a> and distributed under 
            the <a href='http://www.gnu.org/licenses/gpl.html'>GNU General Public License</a> (applications) and <a href='http://www.gnu.org/licenses/lgpl.html'>LGPL</a> (core libraries).
            Assets from the original games remain under their original copyright. 
            Doomsday logo created by Daniel Swanson.
            Website design by Jaakko Ker&auml;nen &copy; 2017. 
        </div>
    </div>");
}
