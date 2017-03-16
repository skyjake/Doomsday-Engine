<?php

require_once('class.session.php');
require_once(DENG_API_DIR.'/include/builds.inc.php');
    
function reformat_date($date_text)
{
    $date = date_parse($date_text);
    $ts = mktime($date['hour'], $date['minute'], $date['second'],
                 $date['month'], $date['day'], $date['year']);
    if (date('Y') != $date['year']) {
        $fmt = '%Y %B %e';
    }
    else {
        $fmt = '%B %e';
    }
    return strftime($fmt, $ts);        
}

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
</head>\n");    
}

function generate_page_title($title)
{
    //<h1>$title</h1>
    echo("<div id='page-title'></div>\n");
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
        $fext = "$plat[cpu_bits]-bit ".$ext;
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
        $title .= " &mdash; $fext";        
    }
    $full_title = "Doomsday ".human_version($version, $build['build'], build_type_text($build['type']))
        ." for ".$plat['name']." (".$plat['cpu_bits']."-bit)";
    $download_url = 'http://api.dengine.net/1/builds?dl='.$file['name'];
    
    if ($plat['os'] != 'any') {
        $metadata .= $plat['name'].' (or later)'; //' &middot; '.$plat['cpu_bits'].'-bit';
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
    <div class='heading'><a href='/manual'>User Guide</a></div>
    <ul>
        <li><a href='/manual/getting_started'>Getting started</a></li>
        <li><a href='/manual/multiplayer'>Multiplayer</a></li>
    </ul>
    </div>
    <div class='partition'>
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
        <li><a href='recent_posts'>Blog</a></li>
        <li><a href='http://twitter.com/@dengteam'>@dengteam</a></li>
        <li><a href='https://github.com/skyjake/Doomsday-Engine.git'>GitHub</a></li>
    </ul>
    <div class='heading'><a href='donate' title='Donate to support the Doomsday Engine Project'>Donate &#9825;</a></div>
</div></div>");
}

function generate_blog_post_cached($post, $css_class)
{
    $nice_date = reformat_date($post->date);
    
    /*$html = '<div class="block"><article class="'.$css_class
        .' content"><header><h1><a href="'.$post->url.'">'
        .$post->title.'</a></h1>';

    $html .= '<p><time datetime="'.$post->date.'" pubdate>'.$nice_date
        .'</time> &mdash; '.$post->author->name.'</p></header><br />';

    $html .= '<div class="articlecontent">'.$post->content.'</div></article>';
    $html .= '<div class="links">'.$source_link.'</div></div>';*/
    
    $html = "<a class='blog-link' href='$post->url'>$post->title</a> "
        ."<time datetime='$post->date' pubdate>&middot; $nice_date</time>";
    
    cache_echo('<li>'.$html.'</li>');    
}

function generate_sitemap()
{
    // Check for a cached sitemap.
    $ckey = cache_key('home', 'sitemap');    
    if (!cache_try_load($ckey)) {            
        // Fetch the cached news and dev blog posts.
        cache_try_load(cache_key('news', 'news'), -1);
        $news = json_decode(cache_get());            
        cache_try_load(cache_key('news', 'dev'), -1);
        $dev = json_decode(cache_get());
        $news_count = min(3, count($news->posts));
        $dev_count  = min(3, count($dev->posts));

        cache_clear();
    
        // Contact the BDB for a list of the latest builds.
        $db = Session::get()->database();
        $result = db_query($db, "SELECT build, version, type, UNIX_TIMESTAMP(timestamp) FROM "
            .DB_TABLE_BUILDS." ORDER BY timestamp DESC");    
        $new_threshold = time() - 2 * 24 * 3600;
        $count = 4;
        $build_list = "<ul class='sitemap-list'>";
        while ($row = $result->fetch_assoc()) {            
            $link = '/build'.$row['build'];
            $version = omit_zeroes($row['version']);
            $label = "$version ".ucwords(build_type_text($row['type']))
                ." [#".$row['build']."]";
            $title = "Build report for $label";
            $ts = (int) $row['UNIX_TIMESTAMP(timestamp)'];
            $date = gmstrftime('&middot; %B %e', $ts);
            $css_class = ($ts > $new_threshold)? ' class="new-build"' : '';
        
            $build_list .= "  <li${css_class}><a title='$title' href='$link'>$label</a> <time>$date</time></li>\n";
            
            if (--$count == 0) break;
        }
        $build_list .= "<li><a href='/builds'>Autobuilder Index</a></li>\n"
            ."<li><a href='http://api.dengine.net/1/builds?format=feed'>RSS Feed</a></li></ul>\n";

        cache_echo(
"<div id='site-map'>
    <ul class='map-wrapper'>
        <li>
            <div class='heading'>Latest News</div>
            <ul class='sitemap-list'>");
                   
        for ($i = 0; $i < $news_count; ++$i) {
            generate_blog_post_cached($news->posts[$i], 'newspost');
        }    
        cache_echo("<li><a href='/blog/category/news/feed/atom' title='Doomsday Engine news via RSS'>RSS Feed</a></li>");
                    
        cache_echo(
"            </ul>
        </li>
        <li>
            <div class='heading'>Blog Posts</div>
            <ul class='sitemap-list'>\n");

        for ($i = 0; $i < $dev_count; ++$i) {
            generate_blog_post_cached($dev->posts[$i], 'blogpost');
        }        
        cache_echo("<li><a href='/blog/category/dev/feed/atom' title='Doomsday Engine development blog via RSS'>RSS Feed</a></li>");
                    
        cache_echo(
"            </ul></li>
        <li>
            <div class='heading'>Recent Builds</div>
            $build_list
        </li>
        <li>
            <div class='heading'>Multiplayer Games</div>
            <ul class='sitemap-list'><li>No servers</li></ul>
        </li>
        <li>
            <div class='heading'>User Manual</div>
        </li>
        <li>
            <div class='heading'>Reference Guide</div>
        </li>
    </ul>
    <div id='credits'>
        Doomsday Engine is <a href='https://github.com/skyjake/Doomsday-Engine.git'>open 
        source software</a> and distributed under 
        the <a href='http://www.gnu.org/licenses/gpl.html'>GNU General Public License</a> (applications) and <a href='http://www.gnu.org/licenses/lgpl.html'>LGPL</a> (core libraries).
        Assets from the original games remain under their original copyright. 
        Doomsday logo created by Daniel Swanson.
        <a href='/'>dengine.net</a> website design by Jaakko Ker&auml;nen &copy; 2017. 
    </div>
</div>");
                
        cache_store($ckey);
    }
    cache_dump();
}
