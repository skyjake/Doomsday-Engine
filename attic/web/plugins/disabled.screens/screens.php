<?php
/**
 * @file screens.php
 *
 * @section License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * @author Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
 */

includeGuard('ScreensPlugin');

require_once(DIR_CLASSES.'/requestinterpreter.interface.php');
require_once(DIR_CLASSES.'/visual.interface.php');

class ScreensPlugin extends Plugin implements Actioner, RequestInterpreter
{
    public static $name = 'Screenshots';

    public static $baseRequestName = 'screenshots';

    private $_displayOptions = VDF_TOPPANEL_SLIM;
    private $_fileID = NULL;

    public function __construct() {}

    public function title()
    {
        return self::$name;
    }

    /**
     * Implements RequestInterpreter
     */
    public function InterpretRequest($request)
    {
        $url = urldecode($request->url()->path());

        // @kludge skip over the first '/' in the home URL.
        $url = substr($url, 1);

        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($url, '.'))
            $url = substr($url, 0, strrpos($url, '.'));

        // Tokenize the request URL so we can begin to determine what to do.
        $tokens = explode('/', $url);

        if(count($tokens) && !strcasecmp(self::$baseRequestName, $tokens[0]))
        {
            FrontController::fc()->enqueueAction($this, NULL);
            return true; // Eat the request.
        }

        return false; // Not for us.
    }

    private function generateJavascript($images, $jcarouselName='mycarousel')
    {
?><script type="text/javascript">
var <?=$jcarouselName?>_itemList = [<?php

        foreach($images as $img => $record)
        {
            echo "{url: \"". $record['url'] ."\", title: \"". $record['caption'] ."\"},\n";
        }

?>];

function <?=$jcarouselName?>_itemLoadCallback(carousel, state)
{
    for(var i = carousel.first; i <= carousel.last; i++)
    {
        if(carousel.has(i)) continue;

        if(i > <?=$jcarouselName?>_itemList.length) break;

        // Create an object from HTML
        var item = jQuery(<?=$jcarouselName?>_getItemHTML(<?=$jcarouselName?>_itemList[i-1])).get(0);

        // Apply thickbox
        tb_init(item);

        carousel.add(i, item);
    }
};

function <?=$jcarouselName?>_getItemHTML(item)
{
    var url_m = item.url.replace(/_s.jpg/g, '_m.jpg');
    return '<a href="' + url_m + '" title="' + item.title + '"><img src="' + item.url + '" width="75" height="75" border="0" alt="' + item.title + '" /></a>';
};

jQuery(document).ready(function()
{
    jQuery(<?="'#$jcarouselName'"?>).jcarousel(
    {
        size: <?=$jcarouselName?>_itemList.length,
        wrap: 'circular',
        itemLoadCallback: {onBeforeAnimation: <?=$jcarouselName?>_itemLoadCallback}
    });
});

</script><?php
    }

    private function generateHTML($heading='', $jcarouselName='mycarousel', $jcarouselSkin='deng')
    {
?><h2><?=$heading?></h2><ul id="<?=$jcarouselName?>" class="jcarousel-skin-<?=$jcarouselSkin?>"></ul><?php
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        $fc = &FrontController::fc();

        $fc->outputHeader($this->title());

        $imgDir = '/images/screenshots';
        $doomImages = array(
            array('url' => $imgDir."/doom/1.jpg", 'caption' => "Title1"),
            array('url' => $imgDir."/doom/2.jpg", 'caption' => "Title2"),
            array('url' => $imgDir."/doom/3.jpg", 'caption' => "Title3"),
            array('url' => $imgDir."/doom/4.jpg", 'caption' => "Title4"),
            array('url' => $imgDir."/doom/5.jpg", 'caption' => "Title5"),
            array('url' => $imgDir."/doom/6.jpg", 'caption' => "Title6"),
            array('url' => $imgDir."/doom/7.jpg", 'caption' => "Title7"));
        $this->generateJavascript($doomImages, 'doomCarousel');

        $hereticImages = array(
            array('url' => $imgDir."/heretic/1.jpg", 'caption' => "Title1"),
            array('url' => $imgDir."/heretic/2.jpg", 'caption' => "Title2"),
            array('url' => $imgDir."/heretic/3.jpg", 'caption' => "Title3"),
            array('url' => $imgDir."/heretic/4.jpg", 'caption' => "Title4"));
        $this->generateJavascript($hereticImages, 'hereticCarousel');

        $hexenImages = array(
            array('url' => $imgDir."/hexen/1.jpg", 'caption' => "Title1"));
        $this->generateJavascript($hexenImages, 'hexenCarousel');

        $fc->beginPage($this->title());

?><div id="screenshots">
<script type="text/javascript">
tb_pathToImage = "/external/thickbox/loading-thickbox.gif";
</script>
<?php

        $this->generateHTML('Doom', 'doomCarousel');
        $this->generateHTML('Heretic', 'hereticCarousel');
        $this->generateHTML('Hexen', 'hexenCarousel');

?></div><?php

        $fc->endPage();
    }
}
