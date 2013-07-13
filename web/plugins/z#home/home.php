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
                var html = '<header>'
                         + '<h1><a href="/masterserver" title="View the complete list in the server browser">'
                         + (serverCount > 0? ('Most active servers '
                                              + '<label title="Result limit">' + minItems + '</label>/<label title="Total number of servers">' + serverCount + '</label>')
                                           : 'No active servers')
                         + '</a></h1>'
                         + '<p>' + niceDate + ' @ <label title="Time of the last server announcement">' + d.toLocaleTimeString() + '</label></p></header>';

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

            var serverMetadataLabel = 'Address: ' + info.children('ip').text() + ':' + info.children('port').text()
                                    + ' Open: ' + info.children('open').text();

            // Any required addons?.
            /*var addonArr = array_filter(explode(';', info['pwads']));
            if(count(addonArr))
            {
                serverMetadataLabel .= 'Add-ons: '. implode(' ', addonArr);
            }*/

            var gameInfo = $(info.children('gameinfo'));
            var gameMetadataLabel = 'Map: ' + gameInfo.children('map').text() + ' Setup: ' + gameInfo.children('setupstring').text();

            var playerCountLabel = 'Number of players currently in-game';
            var playerMaxLabel = 'Maximum number of players';

            var html = '<span class="player-summary"><label title="' + playerCountLabel + '">' + gameInfo.children('numplayers').text() + '</label> / <label title="' + playerMaxLabel + '">' + gameInfo.children('maxplayers').text() + '</label></span> ';

            html += '<label title="' + serverMetadataLabel + '"><span class="name">' + info.find('name').text() + '</span></label> ';
            html += '<label title="' + gameMetadataLabel + '"><span class="game-mode">' + gameInfo.children('mode').text() + '</span></label>';

            return '<span class="server">' + html + '</span>';
        }
    });
});

(function (e) {
    e.fn.interpretFeed = function (t) {
        var n = {
            feedUri: "http://rss.cnn.com/rss/edition.rss",
            maxItems: 5,
            maxContentChars: 0,
            showContent: true,
            showPubDate: true,
            generateItemHtml: 0
        };
        if (t) {
            e.extend(n, t);
        }
        var r = e(this).attr("id");
        var i;
        //e("#" + r).empty().append('<div style="padding:3px;"><img src="images/loader.gif" /></div>');
        e.ajax({
            url: "http://ajax.googleapis.com/ajax/services/feed/load?v=1.0&num=" + n.maxItems + "&output=json&q=" + encodeURIComponent(n.feedUri) + "&hl=en&callback=?",
            dataType: "json",
            success: function (t) {
                //e("#" + r).empty();
                var html = "";
                e.each(t.responseData.feed.entries, function (e, t) {
                    html += '<li>' + n.generateItemHtml(n, t) + '</li>';
                });
                e("#" + r).append('<ol style="list-style-type: none;">' + html + "</ol>");
            }
        })
    }
})(jQuery)

$(document).ready(function () {
    $('#column1').interpretFeed({
        feedUri: 'http://dengine.net/forums/rss.php?mode=news',
        maxItems: 3,
        generateItemHtml: function (n, t) {
            var html = '<article class="block newspost"><header><h1><a href="' + t.link + '" title="Discuss &#39;' + t.title + '&#39; in the user forum">' + t.title + '</a></h1>';
            if (n.showPubDate) {
                var d = new Date(t.publishedDate);
                var niceDate = $.datepicker.formatDate('MM d, yy', d);
                html += '<p><time datetime="' + d.toISOString() + '" pubdate>' + niceDate + '</time></p>';
            }
            html += '</header>';
            if (n.showContent) {
                /*if (n.DescCharacterLimit > 0 && t.content.length > n.DescCharacterLimit) {
                    html += '<div>' + t.content.substr(0, n.DescCharacterLimit) + "...</div>";
                }
                else*/ {
                    html += t.content;
                }
            }
            html += '</article>';
            return html;
        }
    });

    $('#column1').interpretFeed({
        feedUri: 'http://dl.dropboxusercontent.com/u/11948701/builds/events.rss',
        maxItems: 3,
        showContent: true,
        generateItemHtml: function (n, t) {
            var html = '<article class="block buildevent"><header><h1><a href="' + t.link + '" title="Read more about ' + t.title + ' in the repository">' + t.title + '</a></h1>';
            var d = new Date(t.publishedDate);
            var niceDate = $.datepicker.formatDate('MM d, yy', d);
            if (n.showPubDate) {
                html += '<p><time datetime="' + d.toISOString() + '" pubdate>' + niceDate + '</time></p>';
            }
            html += '</header>';
            if (n.showContent) {
                html += 'Build report for ' + t.title.toLowerCase() + ' started on ' + niceDate + '.';
            }
            html += '</article>';
            return html;
        }
    });

    $('#column2').interpretFeed({
        feedUri: 'http://dengine.net/forums/rss.php?f=24',
        maxItems: 3,
        generateItemHtml: function (n, t) {
            var html = '<article class="block blogpost"><header><h1><a href="' + t.link + '" title="Discuss &#39;' + t.title + '&#39; in the user forum">' + t.title + '</a></h1>';
            if (n.showPubDate) {
                var d = new Date(t.publishedDate);
                var niceDate = $.datepicker.formatDate('MM d, yy', d);
                html += '<p><time datetime="' + d.toISOString() + '" pubdate>' + niceDate + '</time></p>';
            }
            html += '</header>';
            if (n.showContent) {
                /*if (n.DescCharacterLimit > 0 && t.content.length > n.DescCharacterLimit) {
                    html += '<div>' + t.content.substr(0, n.DescCharacterLimit) + "...</div>";
                }
                else*/ {
                    html += t.content;
                }
            }
            html += '</article>';
            return html;
        }
    });
});

</script>
<div id="column1" class="twocolumn"></div>
<div id="column2" class="twocolumn"></div>
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
