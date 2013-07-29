<?php
/** @file home.php Implements the dengine.net homepage.
 *
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

        includeHTML('latestversion', 'z#home');

?><div id="contentbox"><?php

        //includeHTML('introduction', 'z#home');

?><script>
(function (e) {
    e.fn.interpretMasterServerStatus = function (t) {
        var n = {
            serverUri: "http://dengine.net/master.php?xml",
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
            else if(mapId.substring(0, 1) == 'E' && mapId.substring(2, 1) == 'M')
            {
                mapId = 'episode ' + mapId.substring(1, 1) + ' map ' + mapId.substring(3, 1);
            }
            else
            {
                mapid = '\'' + mapId + '\'';
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

            var gameMetadataLabel = 'Game; ' + gameMode + '. Server configuration; ' + mapId + ' game-rules; ' + gameRuleStr;

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

(function (e) {
    e.fn.interpretFeed = function (t) {
        var n = { feedUri:          'http://rss.cnn.com/rss/edition.rss',
                  dataType:         'xml',
                  maxItems:         5,
                  clearOnSuccess:   true,
                  useGoogleApi:     false,
                  generateItemHtml: 0
        };
        if(t) {
            e.extend(n, t);
        }
        var r = e(this).attr('id');

        if(n.useGoogleApi)
        {
            e.ajax({
                url: 'http://ajax.googleapis.com/ajax/services/feed/load?v=1.0&num=' + n.maxItems + '&output=json&q=' + encodeURIComponent(n.feedUri) + '&hl=en&callback=?',
                dataType: n.dataType,
                success: function (t) {

                    if(n.clearOnSuccess)
                    {
                        e("#" + r).empty();
                    }

                    var html = "";
                    e.each(t.responseData.feed.entries, function (e, t) {
                        html += n.generateItemHtml(n, t);
                    });
                    e("#" + r).append('<ol style="list-style-type: none;">' + html + "</ol>");
                }
            });
        }
        else
        {
            e.ajax({
                url: n.feedUri,
                dataType: n.dataType,
                success: function (t) {

                    if(n.clearOnSuccess)
                    {
                        e("#" + r).empty();
                    }

                    var xml = $(t);

                    var html = "";
                    xml.find('item').slice(0, n.maxItems).each(function() {
                        var self = $(this),
                        item = { title:       self.children('title').text(),
                                 link:        self.children('link').text(),
                                 author:      self.children('author').text(),
                                 pubDate:     self.children('pubDate').text(),
                                 atomSummary: self.children('atom\\:summary').text(),
                                 description: self.children('description').text(),
                                 author:      self.children('author').text()
                        }
                        html += n.generateItemHtml(n, item);
                    });

                    e('#' + r).append('<ol style="list-style-type: none;">' + html + '</ol>');
                }
            });
        }
    }
})(jQuery)

$(document).ready(function () {
    $('#recentbuilds').interpretFeed({
        feedUri: 'http://dl.dropboxusercontent.com/u/11948701/builds/events.rss',
        dataType: 'xml',
        maxItems: 3,
        generateItemHtml : function(n, t) {
            var html = '<a href="' + t.link + '" title="' + t.title.toLowerCase() + ' (read more in the repository)">' + t.title + ' completed</a>';
            var d = new Date(t.pubDate);
            var niceDate = $.datepicker.formatDate('MM d, yy', d);
            html += ' ' + niceDate;
            d.setDate(d.getDate() + 2);
            var isNew = (new Date() < d);
            return '<li' + (isNew? ' class="new"' : '') + '>' + html + '</li>';
        }
    });

    /*$('#column1').interpretFeed({
        feedUri: 'http://dl.dropboxusercontent.com/u/11948701/builds/events.rss',
        dataType: 'xml',
        maxItems: 3,
        clearOnSuccess: false,
        generateItemHtml: function (n, t) {
            var html = '<div class="block"><article class="buildevent"><header><h1><a href="' + t.link + '" title="Read more about ' + t.title + ' in the repository">' + t.title + '</a></h1>';

            var d = new Date(t.pubDate);
            var niceDate = $.datepicker.formatDate('MM d, yy', d);
            html += '<p><time datetime="' + d.toISOString() + '" pubdate>' + niceDate + '</time></p>';

            html += '</header>';
            html += t.atomSummary;
            html += '</article>';
            html += '<div class="links"><a href="' + n.feedUri + '" class="link-rss" title="Doomsday Engine build events are reported via RSS">All builds</a></div></div>';
            return '<li>' + html + '</li>';
        }
    });*/

    $('#column1').interpretFeed({
        feedUri: 'http://dengine.net/forums/rss.php?mode=news',
        dataType: 'json',
        clearOnSuccess: false,
        useGoogleApi: true,
        maxItems: 1,
        generateItemHtml: function (n, t) {
            var html = '<div class="block"><article class="newspost content"><header><h1><a href="' + t.link + '" title="&#39;' + t.title + '&#39; (full article in the user forums)">' + t.title + '</a></h1>';

            var d = new Date(t.publishedDate);
            var niceDate = $.datepicker.formatDate('MM d, yy', d);
            html += '<p><time datetime="' + d.toISOString() + '" pubdate>' + niceDate + '</time></p>';

            html += '</header>';
            html += t.content;
            html += '</article>';
            html += '<div class="links"><a href="' + n.feedUri + '" class="link-rss" title="Doomsday Engine news via RSS">All news</a></div></div>';
            return '<li>' + html + '</li>';
        }
    });

    $('#column2').interpretFeed({
        feedUri: 'http://dengine.net/forums/rss.php?f=24',
        dataType: 'json',
        clearOnSuccess: false,
        useGoogleApi: true,
        maxItems: 1,
        generateItemHtml: function (n, t) {
            var html = '<div class="block"><article class="blogpost content"><header><h1><a href="' + t.link + '" title="&#39;' + t.title + '&#39; (full article in the user forums)">' + t.title + '</a></h1>';

            var d = new Date(t.publishedDate);
            var niceDate = $.datepicker.formatDate('MM d, yy', d);
            html += '<p><time datetime="' + d.toISOString() + '" pubdate>' + niceDate + '</time></p>';

            html += '</header>';
            html += t.content;
            html += '</article>';
            html += '<div class="links"><a href="' + n.feedUri + '" class="link-rss" title="Doomsday Engine development blog via RSS">All blogs</a></div></div>';
            return '<li>' + html + '</li>';
        }
    });
});
</script>
<aside role="complementary" class="block">
<div id="status">
<div class="twocolumn"><article>
<header><h1><a href="/builds" title="View the complete index in the build repository">Most recent builds</a></h1>
<p><script>
<!--
var niceDate = $.datepicker.formatDate('MM d, yy', new Date());
document.write(niceDate);
//-->
</script></p></header><div id="recentbuilds">Contacting build repository...</div></article></div>
<div class="twocolumn"><article id="activeservers"></article></div>
<div class="clear"></div>
</div></aside>
<div id="column1" class="twocolumn collapsible"></div>
<div id="column2" class="twocolumn collapsible"></div>
<div class="clear"></div>
</div>
<?php

?><div><div id="downloadbox" class="asidebox"><?php

        includeHTML('getitnow', 'z#home');

?></div><?php

?><div id="socialbookmarkbox" class="asidebox"><?php

       includeHTML('socialbookmarks', 'z#home');

?></div></div><?php

        $fc->endPage();
    }
}
