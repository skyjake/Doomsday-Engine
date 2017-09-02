<?php

require_once('config.inc.php');
require_once(DENG_LIB_DIR.'/class.sitedata.php');
require_once(DENG_LIB_DIR.'/utils.inc.php');
require_once(DENG_LIB_DIR.'/sitemap.inc.php');
        
function generate_page_header($title = NULL)
{
    if ($title) {
        $title = " | ".$title;    
    }
    $bg_index = time()/3600 % 10;
    $bg_rotation_css = 
        "body { background-image: url(\"theme/images/site-background${bg_index}.jpg\"); } #page-title { background-image: url(\"theme/images/site-banner${bg_index}.jpg\"); }";
    echo(
"<!DOCTYPE html>
<html lang='en'>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <meta charset='UTF-8'>
  <link href='".SITE_ROOT."/theme/stylesheets/site.css' rel='stylesheet' type='text/css'>
  <style>$bg_rotation_css</style>
  <title>Doomsday Engine$title</title>
  ".webfont_loader_header()
  ."</head>\n");    
}

function generate_page_title($title)
{
    //<h1>$title</h1>
    echo("<div id='page-title'></div>\n");
}

function is_release_candidate_available($platform)
{
    $got_rc = SiteData::get()->data()['downloads']['got_rc'];
    return $got_rc[$platform];
}

function release_notes_link($platform)
{
    $stable_ver = omit_zeroes(SiteData::get()->data()
                              ['latest_stable_version'][$platform]);
    $link = DENG_MANUAL_URL."/version/$stable_ver";
    return "See <a href='".$link."'>release notes for $stable_ver</a> in the Manual.";
}

function generate_download_button()
{
    // Check the latest build.
    $dl_link = platform_download_link();
    $button_label =  omit_zeroes(SiteData::get()->data()
                                 ['latest_stable_version'][user_download_platform()]);    
    if (!$button_label) {
        $button_label = 'Download';
    }
    echo("<a href='$dl_link' class='button' "
            ."title='Download latest stable release'>$button_label <span "
            ."class='downarrow'>&#x21E3;</span></a>");
}

function generate_download_badge($file)
{
    $version = $file['version'];
    $build = $file['build'];
    $build_type = ucwords(build_type_text($file['type']));
    $platform = $file['platform'];
    $fext = $file['fext'];
    
    if ($file['type'] == BT_STABLE) {
        $title = "Doomsday ".omit_zeroes($version);        
    }
    else {
        $title = omit_zeroes($version).' '.$build_type." #$build";
    }
    if ($platform == 'source') {
        $title .= " (Source)";
    }
    else {
        $title .= " &mdash; $fext";        
    }
    $full_title = "Doomsday "
        .human_version($version, $build, build_type_text($file['type']))
        ." for ".$file['platform_name']." (".$file['cpu_bits']."-bit)";
    $download_url = DENG_API_URL.'/builds?dl='.$file['filename'];
    
    $metadata = '';
    if ($file['os'] != 'any') {
        $metadata .= $file['platform_name'].' (or later)'; //' &middot; '.$plat['cpu_bits'].'-bit';
    }
    else {
        $metadata .= "Source code .tar.gz";
        $full_title = "source code for Doomsday "
            .human_version($version, $build, build_type_text($file['type']));
    }
    $metadata .= "<time datetime='$file[timestamp]'> &middot; "
        .reformat_date($file['timestamp']).'</time> '; 
    
    echo('<p><div class="package-badge">'
        ."<a class='package-dl' href='$download_url' "
        ."title=\"Download $full_title\"><div class='package-name'>$title</div><div class='package-metadata'>"
        .$metadata
        ."</div></a></div></p>\n");
}

function generate_badges($platform, $type)
{
    $files = SiteData::get()->data()['downloads']['badges']
                                    [$platform][build_type_text($type)];    
    foreach ($files as $file) {
        generate_download_badge($file);
    }
}

function generate_sidebar()
{
    echo(
"<div id='sidebar'>
    <div class='partition'>
    <div class='heading'><a href='".SITE_ROOT."'>About Doomsday</a></div>
    <div class='heading'>Downloads</div>
    <ul>
        <li><a href='windows'>Windows</a></li>
        <li><a href='macos'>macOS</a></li>
        <li><a href='linux'>Linux</a></li>
        <li><a href='source'>Source</a></li>
        <li><a href='/builds'>Recent builds</a></li>
    </ul>
    <div class='heading'><a href='addons'>Add-ons</a></div>
    <div class='heading'><a href='https://manual.dengine.net'>User Guide</a></div>
    <ul>
        <li><a href='https://manual.dengine.net/getting_started'>Getting started</a></li>
        <li><a href='https://manual.dengine.net/multiplayer'>Multiplayer</a></li>
    </ul>
    </div>
    <div class='partition'>
    <div class='heading'>Community</div>
    <ul>
        <li><a href='/talk'>Forums</a></li>
        <li><a href='/support'>Tech support</a></li>
        <li><a href='https://manual.dengine.net/guide/other_ports'>Other ports</a></li>            
        <li><a href='https://doomwiki.org/wiki/Entryway' title='DoomWiki.org'>Doom Wiki</a></li>
        <li><a href='http://facebook.com/doomsday.engine'><span class='websymbol'>f</span> Facebook</a></li>
    </ul>
    <div class='heading'>Development</div>
    <ul>
        <li><a href='https://github.com/skyjake/Doomsday-Engine.git'><span class='websymbol'>S</span> GitHub</a></li>
        <li><a href='https://tracker.dengine.net/projects/deng'>Bug Tracker</a></li>
        <li><a href='https://tracker.dengine.net/projects/deng/roadmap'>Roadmap</a></li>
        <li><a href='recent_posts'>Blog</a></li>
        <li><a href='http://twitter.com/@dengteam'><span class='websymbol'>t</span> @dengteam</a></li>
    </ul>
    <div class='heading'><a href='donate' title='Donate to support the Doomsday Engine Project'>Donate &#9825;</a></div>
</div></div>");
}
