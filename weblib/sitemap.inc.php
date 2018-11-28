<?php

require_once('utils.inc.php');
require_once('class.sitedata.php');

define('RECENT_THRESHOLD', 3600*72);

function generate_sitemap_blog_post($post, $css_class)
{
    $nice_date = reformat_date($post->date);    
    $html = "<a class='blog-link' href='$post->url'>$post->title</a> "
        ."<time datetime='$post->date' pubdate>&middot; $nice_date</time>";    
    $itemclass = '';
    if (time() - RECENT_THRESHOLD < timestamp_from_date($post->date)) {
        $itemclass = 'recent';
    }    
    echo("<li class='$itemclass'>".$html.'</li>');    
}

function generate_sitemap()
{
    $site_root = SITE_ROOT;    
    $sitedata = SiteData::get()->data()['sitemap_info'];
    
    // Fetch the cached news and dev blog posts.
    cache_try_load(cache_key('blog', 'news'), -1);
    $news = json_decode(cache_get());            
    cache_try_load(cache_key('blog', 'dev'), -1);
    $dev = json_decode(cache_get());
    $news_count = min(4, count($news->posts));
    $dev_count  = min(3, count($dev->posts));
    
    // Check any recently announced servers.
    $recent_servers = '';
    
    foreach ($sitedata['servers'] as $server) {
        $recent_servers .= "<li class='recent-server'><span class='server-players'>$server[pnum] / $server[pmax]</span> &middot; "
            ."<span class='server-name'>$server[name]</span> &middot; <span class='server-game'>$server[game_id]</span></li>";
    }
    if (!$recent_servers) {
        $recent_servers = '<li>No servers</li>';
    }
    
    $build_list = "<ul class='sitemap-list'>";
    foreach ($sitedata['builds'] as $build) {
        $link = $site_root.'/build'.$build['build'];
        $version = $build['version'];
        $label = "$version ".$build['type']." [#".$build['build']."]";
        $title = "Build report for $label";
        $date = $build['date'];
        $css_class = ($build['is_recent'])? ' class="recent"' : '';
    
        $build_list .= "  <li${css_class}><a title='$title' href='$link'>$label</a> <time>$date</time></li>\n";            
    }
    $build_list .= "<li><a href='$site_root/builds'>Autobuilder Index</a></li>\n"
            ."<li><a href='http://api.dengine.net/1/builds?format=feed'><span class='websymbol'>B</span> RSS Feed</a></li></ul>\n";

    echo(
"<div id='site-map'>
    <ul class='map-wrapper'>
        <li>
            <div class='heading'>Latest News</div>
            <ul class='sitemap-list'>");
                   
    for ($i = 0; $i < $news_count; ++$i) {
        generate_sitemap_blog_post($news->posts[$i], 'newspost');
    }    
    echo("<li><a href='http://blog.dengine.net/category/news/feed/atom' title='Doomsday Engine news via RSS'><span class='websymbol'>B</span> RSS Feed</a></li>");
                    
    echo(
"            </ul>
        </li>
        <li>
            <div class='heading'>Blog Posts</div>
            <ul class='sitemap-list'>\n");

    for ($i = 0; $i < $dev_count; ++$i) {
        generate_sitemap_blog_post($dev->posts[$i], 'blogpost');
    }        
    echo("<li><a href='http://blog.dengine.net/category/dev/feed/atom' title='Doomsday Engine development blog via RSS'><span class='websymbol'>B</span> RSS Feed</a></li>");
                    
    echo(
"            </ul></li>
        <li>
            <div class='heading'>Recent Builds</div>
            $build_list
        </li>
        <li>
            <div class='heading'>Multiplayer Games</div>
            <ul class='sitemap-list'>$recent_servers</ul>
        </li>
        <li>
            <div class='heading'>User Manual</div>
            <ul class='sitemap-list'>
                <li><a href='https://manual.dengine.net/getting_started'>Getting started</a></li>            
                <li><a href='https://manual.dengine.net/multiplayer'>Multiplayer</a></li>            
                <li><a href='https://manual.dengine.net/version'>Version history</a></li>            
            </ul>
        </li>
        <li>
            <div class='heading'>Reference Guide</div>
            <ul class='sitemap-list'>
                <li><a href='https://manual.dengine.net/fs'>Packages &amp; assets</a></li>            
                <li><a href='https://manual.dengine.net/ded'>DED definitions</a></li>            
                <li><a href='https://manual.dengine.net/script'>Scripting</a></li>            
            </ul>
        </li>
    </ul>
    <div id='credits'>
        Doomsday Engine is <a href='https://github.com/skyjake/Doomsday-Engine.git'>open 
        source software</a> and is distributed under 
        the <a href='http://www.gnu.org/licenses/gpl.html'>GNU General Public License</a> (applications) and <a href='http://www.gnu.org/licenses/lgpl.html'>LGPL</a> (core libraries).
        Assets from the original games remain under their original copyright. 
        Doomsday logo created by Daniel Swanson.
        <a href='$site_root'>dengine.net</a> website design by Jaakko Ker&auml;nen &copy; 2017. 
        <p><a href='http://dengine.net/donate'>Donate to support the project</a></p>
    </div>
</div>");
}
