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
        $fc->outputHeader($fc->siteDescription());
        $fc->beginPage($this->title());

        includeHTML('latestversion', 'z#home');

?><div id="contentbox"><?php

        //includeHTML('introduction', 'z#home');

?><script type="text/javascript" >
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
