<?php
/** @file home.php Implements the dengine.net homepage.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright Â© 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

includeGuard('HomePlugin');

require_once(DIR_CLASSES.'/requestinterpreter.interface.php');
require_once(DIR_CLASSES.'/visual.interface.php');
require_once(DENG_API_DIR.'/include/builds.inc.php');

function generate_blog_post($post, $css_class, $source_link)
{
    $date = date_parse($post->date);
    $ts = mktime($date['hour'], $date['minute'], $date['second'],
                 $date['month'], $date['day'], $date['year']);
    $nice_date = strftime('%B %d, %Y', $ts);    
    
    $html = '<div class="block"><article class="'.$css_class
        .' content"><header><h1><a href="'.$post->url.'">'
        .$post->title.'</a></h1>';

    $html .= '<p><time datetime="'.$post->date.'" pubdate>'.$nice_date
        .'</time> &mdash; '.$post->author->name.'</p></header><br />';

    $html .= '<div class="articlecontent">'.$post->content.'</div></article>';
    $html .= '<div class="links">'.$source_link.'</div></div>';
    
    echo('<li>'.$html.'</li>');    
}

class HomePlugin extends Plugin implements Actioner, RequestInterpreter
{
    public static $name = 'Home Plugin';
    private static $title = 'Doomsday Engine';

    public function title()
    {
        return self::$title;
    }

    /// Implements RequestInterpreter
    public function InterpretRequest($request)
    {
        $url = urldecode($request->url()->path());
        // @todo kludge: skip over the first '/' in the home URL.
        $url = substr($url, 1);

        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($url, '.'))
            $url = substr($url, 0, strrpos($url, '.'));

        // Our only action will be to display a page to the user.
        FrontController::fc()->enqueueAction($this, NULL);
        return true; // Eat the request.
    }

    /// Implements Actioner.
    public function execute($args=NULL)
    {
        $fc = &FrontController::fc();
        $fc->outputHeader();
        $fc->beginPage($this->title());
        
        $user_platform = detect_user_platform();
        $ckey = cache_key('home', ['latest_stable', $user_platform]);
        if (cache_try_load($ckey)) {
            cache_dump();
        }
        else {        
            switch ($user_platform) {
                case 'windows':
                    $dl_link = '/windows';
                    break;
                case 'macx':
                    $dl_link = '/mac_os';
                    break;
                case 'linux':
                    $dl_link = '/linux';
                    break;
                default:
                    $dl_link = '/source';
                    break;
            }
            // Find out the latest stable build.
            $db = FrontController::fc()->database();
            $result = db_query($db, "SELECT version FROM ".DB_TABLE_BUILDS
                ." WHERE type=".BT_STABLE." ORDER BY timestamp DESC LIMIT 1");
            if ($row = $result->fetch_assoc()) {
                $button_label = omit_zeroes($row['version']);
            }
            else {
                $button_label = 'Download';            
            }
        
            cache_echo("<div id='latestversion'><h1><div class='download-button'><a href='$dl_link' title='Download latest stable release'>$button_label <span class='downarrow'>&#x21E3;</span></a></div></h1></div>\n");

            cache_dump();
            cache_store($ckey);
        }        
        
        // Fetch the cached news and dev blog posts.
        cache_try_load(cache_key('news', 'news'), -1);
        $news = json_decode(cache_get());        
        cache_try_load(cache_key('news', 'dev'), -1);
        $dev = json_decode(cache_get());
                
        // Figure out how many posts to show.
        $news_size = strlen($news->posts[0]->content);
        $dev_size  = strlen($dev->posts[0]->content);
        $news_count = 1;
        $dev_count = 1;        
        if ($news_size < $dev_size * 0.8) {
            $news_count += 1;
        }        
        else if ($dev_size < $news_size * 0.8) {
            $dev_count += 1;
        }        
        
?><div id="contentbox"><?php

?><script>
(function (e) {
    e.fn.interpretMasterServerStatus = function (t) {
        var n = {
            serverUri: "/master.php?xml",
            maxItems: 3,
            generateServerSummaryHtml: 0
        };
        if (t) {
            e.extend(n, t);
        }
        var r = e(this).attr("id");
        e.ajax({
            url: n.serverUri,
            dataType: 'xml',
            success: function (t) {
                e("#" + r).empty();
                var root = $('masterserver', t);
                var serverList = root.find('serverlist')[0];
                var serverCount = serverList.attributes.getNamedItem('size').nodeValue;

                var d = new Date(root.find('channel>pubdate').text());
                var niceDate = $.datepicker.formatDate('MM d, yy', d);
                var minItems = serverCount < n.maxItems? serverCount : n.maxItems;

                var announceTime = d.toLocaleTimeString();

                var html = '<header>'
                         + '<h1><a href="/masterserver" title="View the complete list in the server browser">'
                         + (serverCount > 0? ('Most active servers '
                                              + '<label title="Result limit">' + minItems + '</label>/<label title="Total number of servers">' + serverCount + '</label>')
                                           : 'No active servers')
                         + '</a></h1>'
                         + '<p>' + niceDate + ' @ <label title="' + announceTime + ' (time of the last server announcement)">' + announceTime + '</label></p></header>';

                if(serverCount) {
                    var idx = 0;
                    var playerCount = 0;

                    html += '<ol class="servers">';
                    for(var i = 0; i < serverList.childNodes.length; ++i) {
                        if(serverList.childNodes[i].nodeName != 'server')
                            continue;

                        var server = $(serverList.childNodes[i]);
                        if(n.maxItems <= 0 || idx < n.maxItems)
                        {
                            html += '<li>' + n.generateServerSummaryHtml(n, server) + '</li>';
                        }

                        playerCount += Number(server.find('gameinfo>numplayers').text());
                        idx++;
                    };

                    html += '</ol>';
                    //html += '<span class="summary"><a href="/masterserver" title="See the full listing in the server browser">' + serverCount + ' ' + (serverCount == 1? 'server':'servers') + ' in total, ' + playerCount + ' active ' + (playerCount == 1? 'player':'players') + '</a></span>';
                }

                e("#" + r).append(html);
            }
        })
    }
})(jQuery)

$(document).ready(function () {
    $('#activeservers').interpretMasterServerStatus({
        generateServerSummaryHtml: function (n, info) {

            var name = info.find('name').text();
            var status = info.children('open').text() == 'yes'? 'open' : 'locked';

            var serverMetadataLabel = 'I.P address ' + info.children('ip').text() + ', port ' + info.children('port').text()
                                    + ', ' + status;

            // Any required addons?.
            /*var addonArr = array_filter(explode(';', info['pwads']));
            if(count(addonArr))
            {
                serverMetadataLabel .= 'Add-ons: '. implode(' ', addonArr);
            }*/

            var gameInfo = $(info.children('gameinfo'));
            var gameMode = gameInfo.children('mode').text();

            var mapId = gameInfo.children('map').text();
            if(mapId.substring(0, 3) == 'MAP')
            {
                mapId = 'map ' + mapId.substring(3);
            }
            else if(mapId.substring(0, 1) == 'E' && mapId.substring(2, 3) == 'M')
            {
                mapId = 'episode ' + mapId.substring(1, 2) + ', map ' + mapId.substring(3, 4);
            }
            else
            {
                mapId = 'map \'' + mapId + '\'';
            }

            var gameRules = gameInfo.children('setupstring').text().split(/[ ,]+/);

            for(var i=0; i < gameRules.length; i++)
            {
                if(gameRules[i] == 'dm')
                    gameRules[i] = 'deathmatch';
                else if(gameRules[i] == 'coop')
                    gameRules[i] = 'co-operative';
                else if(gameRules[i] == 'nomonst')
                    gameRules[i] = 'no monsters';
                else if(gameRules[i] == 'jump')
                    gameRules[i] = 'jump enabled';
            }

            var gameRuleStr = gameRules.join(', ');

            var gameMetadataLabel = 'Game; ' + gameMode + '. Server configuration; ' + mapId + ', game-rules; ' + gameRuleStr;

            var playerCount = gameInfo.children('numplayers').text();
            var playerMax   = gameInfo.children('maxplayers').text();

            var playerCountLabel = playerCount + ' players currently in-game';
            var playerMaxLabel = playerMax + ' player limit';

            var html = '<span class="player-summary"><label title="' + playerCountLabel + '">' + playerCount + '</label> / <label title="' + playerMaxLabel + '">' + playerMax + '</label></span> ';

            html += '<span class="name"><label title="Server name \'' + name + '\', ' + serverMetadataLabel + '">' + name + '</label></span> ';
            html += '<span class="game-mode"><label title="' + gameMetadataLabel + '.">' + gameMode + '</label></span>';

            return '<span class="server">' + html + '</span>';
        }
    });
});
</script>

<aside role="complementary" class="block"><div id="status">

<div class="twocolumn">
<article id='mostrecentbuilds'>
<header><h1><a href="/builds" title="View the complete index in the build repository">Most recent builds</a></h1>
<p><script>
<!--
var niceDate = $.datepicker.formatDate('MM d, yy', new Date());
document.write(niceDate);
//-->
</script></p></header><div id="recentbuilds">
<?php

    // Contact the BDB for a list of the latest builds.
    {
        $db = $fc->database();
        $result = db_query($db, "SELECT build, version, type, UNIX_TIMESTAMP(timestamp) FROM "
            .DB_TABLE_BUILDS." ORDER BY timestamp DESC");    
        $new_threshold = time() - 2 * 24 * 3600;
        $count = 3;
        echo("<ol style=\"list-style-type: none;\">");
        while ($row = $result->fetch_assoc()) {            
            $link = '/build'.$row['build'];
            $label = "Build $row[build] completed";
            $version = omit_zeroes($row['version']);
            $title = "Build report for $version ".ucwords(build_type_text($row['type']))
                ." [#".$row['build']."]";
            $ts = (int) $row['UNIX_TIMESTAMP(timestamp)'];
            $date = gmstrftime('%b %d', $ts);
            $css_class = ($ts > $new_threshold)? ' class="new"' : '';
        
            echo("<li${css_class}><a title='$title' href='$link'>$label</a> $date</li>\n");
            
            if (--$count == 0) break;
        }
        echo("</ol>\n");
 
    /*    generateItemHtml : function(n, t) {
            var html = '<a href="' + t.link + '" title="' + t.title.toLowerCase() + ' (read more in the repository)">' + t.title + ' completed</a>';
            var d = new Date(t.pubDate);
            var niceDate = $.datepicker.formatDate('MM d', d);
            html += ' ' + niceDate;
            d.setDate(d.getDate() + 2);
            var isNew = (new Date() < d);
            return '<li' + (isNew? ' class="new"' : '') + '>' + html + '</li>';
        }*/    
    }
    
?></div>
</article>
</div>

<div class="twocolumn"><article id="activeservers"></article></div>

<div class="centercolumn">
    <?php includeHTML('getitnow', 'z#home'); ?>
</div>

<div class="clear"></div>
</div></aside>

<div id="column1" class="twocolumn collapsible">
    <ol style="list-style-type:none;">
    <?php
    
    for ($i = 0; $i < $news_count; ++$i) {
        generate_blog_post($news->posts[$i], 'newspost', '<a href="/blog/category/news/feed/atom" class="link-rss" title="Doomsday Engine news via RSS">News feed</a>');
    }    
    
    ?>    
    </ol>
</div>
<div id="column2" class="twocolumn collapsible">
    <ol style="list-style-type:none;">
    <?php
    
    for ($i = 0; $i < $dev_count; ++$i) {
        generate_blog_post($dev->posts[$i], 'blogpost', '<a href="/blog/category/dev/feed/atom" class="link-rss" title="Doomsday Engine development blog via RSS">Dev feed</a>');
    }        
    
    ?>
    </ol>
</div>
<div class="clear"></div>

</div>
<?php

?><div><div id="downloadbox" class="asidebox"><?php
        #includeHTML('getitnow', 'z#home');
?></div><?php

?><div id="socialbookmarkbox" class="asidebox"><?php

       includeHTML('socialbookmarks', 'z#home');

?></div></div><?php

        $fc->endPage();
    }
}
