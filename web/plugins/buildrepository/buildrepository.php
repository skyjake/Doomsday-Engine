<?php
/**
 * @file buildrepository.php
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
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
 */

includeGuard('BuildRepositoryPlugin');

require_once(DIR_CLASSES.'/outputcache.class.php');

/**
 * @defgroup platformId  Platform Identifier
 * @ingroup builds
 */
///@{
define('PID_ANY',             0); ///< (Enumeration). Ordinals are meaningless (can change) however must be unique.
define('PID_WIN_X86',         1);
define('PID_MAC10_4_X86_PPC', 2);
define('PID_LINUX_X86',       3);
define('PID_LINUX_X86_64',    4);
///@}

/**
 * @defgroup releaseType  Release Type
 * @ingroup builds
 */
///@{
define('RT_UNSTABLE',         0); ///< (Enumeration). Ordinals are meaningless (can change) however must be unique.
define('RT_CANDIDATE',        1);
define('RT_STABLE',           2);
///@}

require_once('buildevent.class.php');
require_once('packagefactory.class.php');
require_once('buildlogparser.class.php');

function retrieveBuildLogXml(&$buildLogUri)
{
    try
    {
        // Open a new cURL session.
        $cs = curl_init();
        if($cs === FALSE)
            throw new Exception('Failed initializing cURL');

        curl_setopt($cs, CURLOPT_URL, $buildLogUri);
        curl_setopt($cs, CURLOPT_HEADER, false);
        curl_setopt($cs, CURLOPT_RETURNTRANSFER, true);

        $xml = curl_exec($cs);
        if($xml === FALSE)
            $error = curl_error($cs);
        curl_close($cs);
        if($xml === FALSE)
            throw new Exception('Failed retrieving file' + $error);

        return $xml;
    }
    catch(Exception $e)
    {
        /// \todo Store error so users can query.
        //setError($e->getMessage());
        return FALSE;
    }
}

/**
 * Determine if the local cached copy of the remote build log requires an
 * update (i.e., the remote server indicates the file has changed, or our
 * cached copy has been manually purged).
 *
 * @param buildLogUri  (String) Uri locator to the remote build log.
 * @param cacheName  (String) Name of the cache copy of the log.
 * @return  (Boolean) @c true if the cache copy of the log needs an update.
 */
function mustUpdateCachedBuildLog(&$buildLogUri, &$cacheName)
{
    global $FrontController;

    if(!$FrontController->contentCache()->isPresent($cacheName))
        return TRUE;

    // Only query the remote server at most once every five minutes for an
    // updated build log. The modified time of our local cached copy is used
    // to determine when it is time to query (we touch the cached copy after
    // each query attempt).
    $cacheInfo = new ContentInfo();
    $FrontController->contentCache()->getInfo($cacheName, &$cacheInfo);
    if(time() < strtotime('+5 minutes', $cacheInfo->modifiedTime))
        return FALSE;

    // Check the remote time of the log and compare with our local cached copy.
    try
    {
        // Open a new cURL session.
        $cs = curl_init();
        if($cs === FALSE)
            throw new Exception('Failed initializing cURL');

        // We only want the headers.
        curl_setopt($cs, CURLOPT_URL, $buildLogUri);
        curl_setopt($cs, CURLOPT_NOBODY, true);
        curl_setopt($cs, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($cs, CURLOPT_FILETIME, true);

        $xml = curl_exec($cs);
        if($xml !== FALSE)
        {
            $timestamp = curl_getinfo($cs, CURLINFO_FILETIME);
            // Close the cURL session.
            curl_close($cs);

            if($timestamp != -1)
            {
                if($timestamp > $cacheInfo->modifiedTime)
                {
                    // Update necessary.
                    return TRUE;
                }
                else
                {
                    // Touch our cached copy so we can delay checking again.
                    $FrontController->contentCache()->touch($cacheName);
                    return FALSE;
                }
            }
            else
            {
                // Server could/would not supply the headers we are interested in.
                // We'll err on the side of caution and update (our five minute
                // repeat delay mitigates the sting).
                return TRUE;
            }
        }
        else
        {
            $error = curl_error($cs);
            // We are now done with the cURL session.
            curl_close($cs);
            throw new Exception('Failed retrieving file info' + $error);
        }
    }
    catch(Exception $e)
    {
        /// \todo Store error so users can query.
        //setError($e->getMessage());
        return FALSE;
    }
}

function addCommitToGroup(&$groups, $groupName, &$commit)
{
    if(!is_array($groups))
        throw new Exception('Invalid groups argument, array expected');

    $key = array_casekey_exists($groupName, $groups);
    if($key === false)
    {
        $groups[$groupName] = array();
        $group = &$groups[$groupName];
    }
    else
    {
        $group = &$groups[$key];
    }

    $group[] = $commit;
}

function groupBuildCommits(&$build, &$groups)
{
    if(!$build instanceof BuildEvent)
        throw new Exception('Received invalid BuildEvent');
    if(!is_array($groups))
        throw new Exception('Invalid groups argument, array expected');

    foreach($build->commits as &$commit)
    {
        if(!is_array($commit['tags']) || 0 === count($commit['tags']))
        {
            addCommitToGroup($groups, 'Miscellaneous', $commit);
            continue;
        }

        $tags = $commit['tags'];
        reset($tags);
        $firstTag = key($tags);
        addCommitToGroup($groups, $firstTag, $commit);
    }
}

function make_pretty_hyperlink($matches)
{
    $uri = implode('', array_slice($matches, 1));

    if(strlen($uri) > 40)
        $shortUri = substr($uri, 0, 40).'...';
    else
        $shortUri = $uri;

    /// \fixme Do not assume all links are external ones.
    return '<a class="link-external" href="'.$uri.'">'.$shortUri.'</a>';
}

function outputCommitHTML(&$commit)
{
    if(!is_array($commit))
        throw new Exception('Invalid commit argument, array expected');

    // Process the commit message, replacing web URIs with clickable links.
    $message =  preg_replace_callback("/([^A-z0-9])(http|ftp|https)([\:\/\/])([^\\s]+)/",
        "make_pretty_hyperlink", $commit['message']);
    $message = nl2br($message);
    $haveMessage = (bool)(strlen($message) > 0);

    // Compose the supplementary tag list.
    $tagList = "";
    if(is_array($commit['tags']))
    {
        $n = (integer)0;
        foreach($commit['tags'] as $key => $value)
        {
            // Skip the first tag (its used for grouping).
            if($n === 0) continue;

            // Do not output guessed tags (mainly used for grouping).
            if(is_array($value) && isset($value['guessed']) && $value['guessed'] !== 0) continue;

            $tagList .= '<span class="tag">'.$key.'</span>';
        }
    }

    $repoLinkTitle = 'Show changes in the repository for this commit submitted on '. date(DATE_RFC2822, $commit['submitDate']) .'.';

    // Ouput HTML for the commit.
?><span class="metadata"><a href="<?php echo $commit['repositoryUri']; ?>" class="link-external" title="<?php echo $repoLinkTitle; ?>"><?php echo date('Y-m-d', $commit['submitDate']); ?></a></span><p class="heading <?php if($haveMessage) echo 'collapsible'; ?>" <?php if($haveMessage) echo 'title="Toggle commit message display"'; ?>><strong><?php echo $tagList; ?><span class="title"><?php echo $commit['title']; ?></span></strong> by <em><?php echo $commit['author']; ?></em></p><?php

    if($haveMessage)
    {
        ?><div class="commit"><blockquote><?php echo $message; ?></blockquote></div><?php
    }
}

function outputCommitJumpList2(&$groups)
{
    if(!is_array($groups))
        throw new Exception('Invalid groups argument, array expected');

?><ol class="jumplist"><?php
    foreach($groups as $groupName => $group)
    {
        $tagLinkTitle = "Jump to commits tagged '$groupName'";

?><li><a href="#<?php echo $groupName; ?>" title="<?php echo $tagLinkTitle; ?>"><?php echo htmlspecialchars($groupName); ?></a></li><?php
    }
?></ol><?php
}

function outputCommitJumpList(&$groups)
{
    if(!is_array($groups))
        throw new Exception('Invalid groups argument, array expected');

?><div class="jumplist_wrapper"><?php

    // If the list is too long; split it into multiple sublists.
    $groupCount = count($groups);
    if($groupCount > 5)
    {
        $numLists = ceil($groupCount / 5);
        for($i = (integer)0; $i < $numLists; $i++)
        {
            $subList = array_slice($groups, $i*5, 5);
            outputCommitJumpList2(&$subList);
        }
    }
    else
    {
        // Just the one list then.
        outputCommitJumpList2($groups);
    }

?></div><?php
}

/**
 * Process the commit log of the given build event, generating HTML
 * content directly to the output stream.
 *
 * @param build  (object) BuildEvent object to process.
 */
function outputCommitLogHTML(&$build)
{
    if(!$build instanceof BuildEvent)
        throw new Exception('Received invalid BuildEvent');

    if(count($build->commits))
    {
        $groups = array();
        groupBuildCommits($build, &$groups);

        $groupCount = count($groups);
        if($groupCount > 1)
        {
            ksort($groups);

            // Generate a jump list.
            outputCommitJumpList($groups);
        }

        // Generate the commit list itself.
?><hr /><ul><?php

        foreach($groups as $groupName => $group)
        {
?><li><?php

            if($groupCount > 1)
            {
?><strong><span class="tag"><?php echo htmlspecialchars($groupName); ?></span></strong><a name="<?php echo $groupName; ?>"></a><a class="jump" href="#commitindex" title="Back to Commits index">index</a><br /><ol><?php
            }

            foreach($group as &$commit)
            {
?><li <?php if(isset($commit['message']) && strlen($commit['message']) > 0) echo 'class="more"'; ?>><?php

                outputCommitHTML($commit);

?></li><?php
            }

?></ol></li><?php
        }

?></ul><?php
    }
}

/**
 * @param build  (object) BuildEvent to generate a commit log for.
 * @return  (boolean) @c true if a commit log was sent to output.
 */
function outputCommitLog(&$build)
{
    global $FrontController;

    if(!$build instanceof BuildEvent)
        throw new Exception('Received invalid BuildEvent');

    if(count($build->commits) <= 0) return FALSE;

    $commitsCacheName = 'buildrepository/'.$build->uniqueId().'/commits.html';
    try
    {
        $FrontController->contentCache()->import($commitsCacheName);
    }
    catch(Exception $e)
    {
        $OutputCache = new OutputCache();

        $OutputCache->start();
        outputCommitLogHTML($build);
        $content = $OutputCache->stop();

        $FrontController->contentCache()->store($commitsCacheName, $content);

        print($content);
    }

    return TRUE;
}

/**
 * Construct the BuildEvent collection by parsing the build log.
 *
 * @param buildLogUri  (String) Uri locator for the log to be parsed.
 * @param builds  (Array) Collection to be populated with BuildEvents.
 * @return (Boolean) @c TRUE iff successful.
 */
function constructBuilds($buildLogUri, &$builds)
{
    global $FrontController;

    // Is it time to update our cached copy of the build log?
    $logCacheName = 'buildrepository/events.xml';
    if(mustUpdateCachedBuildLog($buildLogUri, $logCacheName))
    {
        // Grab a copy and store it in the local file cache.
        $logXml = retrieveBuildLogXml($buildLogUri);
        if($logXml == FALSE)
            throw new Exception('Failed retrieving build log');

        $FrontController->contentCache()->store($logCacheName, $logXml);
    }

    // Attempt to parse the local cached copy, transforming it into a
    // collection of abstract objects we use to model it.
    try
    {
        $cachedLogXml = $FrontController->contentCache()->retrieve($logCacheName);
        BuildLogParser::parse($cachedLogXml, $builds);
        return TRUE;
    }
    catch(Exception $e)
    {
        return FALSE;
    }
}

class BuildRepositoryPlugin extends Plugin implements Actioner, RequestInterpreter
{
    /// Plugin name.
    public static $name = 'buildrepository';

    /// Feed URIs:
    const XML_FEED_URI = 'http://code.iki.fi/builds/events.xml';
    const RSS_FEED_URI = 'http://code.iki.fi/builds/events.rss';

    /**
     * Plaform (Record):
     *
     * 'id': @see platformId
     * 'name': Symbolic name used to identify this platform.
     * 'nicename': User facing/friendly name for this platform.
     */
    private static $platforms = array(
        PID_WIN_X86         => array('id'=>PID_WIN_X86,         'name'=>'win-x86',         'nicename'=>'Windows'),
        PID_MAC10_4_X86_PPC => array('id'=>PID_MAC10_4_X86_PPC, 'name'=>'mac10_4-x86-ppc', 'nicename'=>'Mac OS (10.4+)'),
        PID_LINUX_X86       => array('id'=>PID_LINUX_X86,       'name'=>'linux-x86',       'nicename'=>'Ubuntu (32bit)'),
        PID_LINUX_X86_64    => array('id'=>PID_LINUX_X86_64,    'name'=>'linux-x86_64',    'nicename'=>'Ubuntu (64bit)'),
        );
    private static $unknownPlatform = array(
        'id'=>PID_ANY, 'name'=>'unknown', 'nicename'=>'Unknown');

    /// Special 'null' package.
    private static $nullPack = NULL;

    /// Builds.
    private $builds;

    /// Package collections.
    private $packages = NULL;
    private $symbolicPackages = NULL;

    /// Weight values for sorting of Packages.
    private static $packageWeights = array(
        'DistributionPackage'            =>0,
        'PluginPackage'                  =>1,
        'UnstableDistributionPackage'    =>2,
        'UnstablePluginPackage'          =>3
    );

    /// @return  Plugin name.
    public function title()
    {
        return self::$name;
    }

    /**
     * Attempt to parse a 'release type' symbolic name/identifier from @a str
     * if it does not match a known type then RT_UNSTABLE is returned.
     * @return  (integer) Unique identifier associated the determined release type.
     */
    public static function parseReleaseType($str)
    {
        if(is_string($str))
        {
            if(!strcasecmp($str, 'unstable')) return RT_UNSTABLE;
            if(!strcasecmp($str, 'candidate')) return RT_CANDIDATE;
            if(!strcasecmp($str, 'stable')) return RT_STABLE;
        }
        // Unknown. Assume 'unstable'.
        return RT_UNSTABLE;
    }

    /**
     * Attempt to parse @a str, comparing with known symbolic platform
     * names. If no match is determined @c PID_ANY is returned.
     *
     * @param str  (String) Textual value to be parsed.
     * @return  (Integer) Determined PlatformId.
     */
    public static function parsePlatformId($str="")
    {
        if(is_string($str))
        {
            foreach(self::$platforms as $plat)
            {
                if(!strcasecmp($plat['name'], $str)) return $plat['id'];
            }
        }
        return PID_ANY; // Unknown.
    }

    /**
     * Retrieve the Platform record associated with @a platformId.
     * If no match is found then the special 'unknown' platform is returned.
     *
     * @param platformId  (Integer) Unique identfier of the platform.
     * @return  (Object) Determined Platform.
     */
    public static function &platform($platformId=PID_ANY)
    {
        $platformId = intval($platformId);
        if(isset(self::$platforms[$platformId])) return self::$platforms[$platformId];
        return self::$unknownPlatform;
    }

    public static function packageWeight(&$pack)
    {
        if($pack instanceof BasePackage)
        {
            if(isset(self::$packageWeights[get_class($pack)]))
                return self::$packageWeights[get_class($pack)];
        }
        return 4;
    }

    public static function packageSorter($packA, $packB)
    {
        // Primarily by platform id.
        if($packA->platformId() !== $packB->platformId()) return $packA->platformId() - $packB->platformId();

        // Secondarily by package type.
        if($packA === $packB) return 0;
        $a = self::packageWeight($packA);
        $b = self::packageWeight($packB);
        if($a === $b) return 0;
        return $a < $b? -1 : 1;
    }

    private function grabAndParseBuildFeedXML()
    {
        $this->builds = array();
        constructBuilds(self::XML_FEED_URI, $this->builds);
    }

    /**
     * Populate the collection with 'static' packages. A static package is
     * one which was not produced by or that which has no persistent build
     * event information.
     *
     * Static packages are primarily those for 'stable' releases.
     *
     * @param packages  (Array) Collection to be populated.
     * @return  (Boolean) @c TRUE iff successful.
     */
    private function populateStaticPackages(&$packages)
    {
        /**
         * \todo Read this information from a config file, we should not
         * expect to edit this file in order to change these...
         */

        // Windows:
        $pack = PackageFactory::newDistribution(PID_WIN_X86, 'Doomsday', '1.8.6',
            'http://sourceforge.net/projects/deng/files/Doomsday%20Engine/1.8.6/deng-inst-1.8.6.exe/download');
        $packages[] = $pack;

        $pack = PackageFactory::newUnstableDistribution(PID_WIN_X86, 'Doomsday', '1.9.0-beta6.9',
            'http://sourceforge.net/projects/deng/files/Doomsday%20Engine/1.9.0-beta6.9/deng-1.9.0-beta6.9-setup.exe/download');
        $packages[] = $pack;

        // Mac OS:
        $pack = PackageFactory::newUnstableDistribution(PID_MAC10_4_X86_PPC, 'Doomsday', '1.9.0-beta6.9',
            'http://sourceforge.net/projects/deng/files/Doomsday%20Engine/1.9.0-beta6.9/deng-1.9.0-beta6.9.dmg/download');
        $packages[] = $pack;

        // Ubuntu:
        /*$pack = PackageFactory::newUnstableDistribution(PID_LINUX_X86_64, 'Doomsday', '1.9.0-beta6.9',
            'http://sourceforge.net/projects/deng/files/');
        $packages[] = $pack;*/

        return TRUE;
    }

    /**
     * Populate the collection with 'symbolic' packages. A symbolic package
     * is one which is not linked to any specific downloadable packs or
     * build events. Instead symbolic packages use abstract names and
     * dynamically resolved download URIs which map to "real" packages.
     *
     * @param packages (Array) Collection to be populated.
     * @return  (Boolean) @c TRUE iff successful.
     */
    private function populateSymbolicPackages(&$packages)
    {
        /**
         * \todo Read this information from a config file, we should not
         * expect to edit this file in order to change these...
         */

        // Windows:
        $plat = $this->platform(PID_WIN_X86);
        $pack = PackageFactory::newUnstableDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        // Mac OS:
        $plat = $this->platform(PID_MAC10_4_X86_PPC);
        $pack = PackageFactory::newUnstableDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        // Ubuntu:
        $plat = $this->platform(PID_LINUX_X86);
        $pack = PackageFactory::newUnstableDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        $plat = $this->platform(PID_LINUX_X86_64);
        $pack = PackageFactory::newUnstableDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        return TRUE;
    }

    private function rebuildPackages()
    {
        // Generate packages from feed events.
        $this->grabAndParseBuildFeedXML();

        // Main collection consists of those produced from the feed and the statics.
        $this->packages = array();

        foreach($this->builds as &$build)
        foreach($build->packages as &$pack)
        {
            $this->packages[] = $pack;
        }

        // Add the 'static' packages.
        $this->populateStaticPackages(&$this->packages);

        // The symbolic packages are kept seperate.
        $this->symbolicPackages = array();
        $this->populateSymbolicPackages(&$this->symbolicPackages);
    }

    /**
     * One-time initialization of the Packages collection.
     */
    private function initPackages()
    {
        // Init the special case 'null' package.
        self::$nullPack = PackageFactory::newNullPackage();

        // Init collections.
        $this->rebuildPackages();
    }

    /**
     * Choose a Package from the collection which satisfies the
     * specified selection filters. If no suitable package can
     * be found the special NullPackage is returned.
     *
     * @param platformId  (Integer) PlatformId filter.
     * @param title  (String) Symbolic name of the package to download. Default="Doomsday".
     * @param unstable  (Boolean) @c true= Only consider 'unstable' packs.
     * @return  (Object) Chosen package.
     */
    private function &choosePackage($platformId=PID_ANY, $title="Doomsday", $unstable=FALSE)
    {
        $unstable = (boolean)$unstable;

        if(isset($this->packages))
        {
            $matchTitle = (boolean)(strlen($title) > 0);

            foreach($this->packages as &$pack)
            {
                if($pack->platformId() !== $platformId) continue;
                if($matchTitle && strcmp($pack->title(), $title)) continue;
                if($unstable != ($pack instanceof AbstractUnstablePackage)) continue;

                // Found something suitable.
                return $pack;
            }
        }

        // Nothing suitable.
        return self::$nullPack;
    }

    /**
     * Implements RequestInterpreter
     */
    public function InterpretRequest($request)
    {
        global $FrontController;

        $uri = urldecode($request->url()->path());
        // @kludge skip over the first '/' in the home URL.
        $uri = substr($uri, 1);
        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($uri, '.'))
            $uri = substr($uri, 0, strrpos($uri, '.'));

        $uriArgs = $request->url()->args();

        // Tokenize the request URL so we can begin to determine what to do.
        $tokens = explode('/', $uri);
        if(!count($tokens)) return false; // How peculiar...

        // A symbolic build name?
        if($tokens[0] === 'latestbuild')
        {
            $this->initPackages();

            // Are we redirecting to the download URI for a specific package?
            $getPackage = FALSE;
            if(isset($uriArgs['platform']))
            {
                // Parse and validate arguments.
                $platformId = $this->parsePlatformId($uriArgs['platform']);
                $unstable = isset($uriArgs['unstable']);

                // Default to downloading Doomsday if a pack is not specified.
                $packTitle = "Doomsday";
                if(isset($uriArgs['pack']))
                    $packTitle = trim($uriArgs['pack']);

                // Try to find a suitable package...
                $pack = &$this->choosePackage($platformId, $packTitle, $unstable);
                if(!($pack instanceof NullPackage))
                {
                    $FrontController->enqueueAction($this, array('getpackage' => $pack));
                    return true; // Eat the request.
                }
            }

            // Redirect to the build repository index.
            $FrontController->enqueueAction($this, array('build' => -1/*dummy value*/));
            return true; // Eat the request.
        }

        // Perhaps a named build or the index?
        if(!strncasecmp('build', $tokens[0], 5))
        {
            $this->initPackages();

            $buildNumber = intval(substr($tokens[0], 5));
            $FrontController->enqueueAction($this, array('build' => $buildNumber));
            return true; // Eat the request.
        }

        return false; // Not for us.
    }

    /**
     * Output build event stream navigational controls.
     *
     * @param prevEvent  (Object) Previous event in the stream (if any).
     * @param nextEvent  (Object) Next event in the stream (if any).
     */
    private function outputBuildStreamNavigation($prevEvent=NULL, $nextEvent=NULL)
    {
        $prevBuildUri = isset($prevEvent)? $prevEvent->composeBuildUri() : '';
        $nextBuildUri = isset($nextEvent)? $nextEvent->composeBuildUri() : '';

?><div class="hnav" id="buildsnav"><span class="title">Build stream navigation:</span><ul><?php

        // Older event link.
        echo '<li>';
        if(!is_null($prevEvent))
            echo "<a href=\"$prevBuildUri\" title=\"View older ".$prevEvent->composeName()."\">";
        echo '&lt; Older';
        if(!is_null($prevEvent))
            echo '</a>';
        echo '</li>';

        // Build Repository link.
        echo '<li><a href="builds" title="Back to the Build Repository">Index</a></li>';

        // Newer event link.
        echo '<li>';
        if(!is_null($nextEvent))
            echo "<a href=\"$nextBuildUri\" title=\"View newer ".$nextEvent->composeName()."\">";
        echo 'Newer &gt;';
        if(!is_null($nextEvent))
            echo '</a>';
        echo '</li>';

?></ul></div><?php
    }

    private function findBuildByUniqueId($uniqueId=0)
    {
        $uniqueId = intval($uniqueId);
        $build = NULL;
        if($uniqueId > 0)
        {
            foreach($this->builds as &$build)
            {
                if($uniqueId === $build->uniqueId()) break;
            }
        }
        return $build;
    }

    private function findNewerBuild($build=NULL)
    {
        $newer = NULL;
        if(!is_null($build) && $build instanceof BuildEvent)
        {
            foreach($this->builds as &$other)
            {
                if($other === $build) continue;

                $otherId = $other->uniqueId();
                if($otherId > $build->uniqueId() &&
                   (is_null($newer) || $otherId < $newer->uniqueId()))
                {
                    $newer = $other;
                }
            }
        }
        return $newer;
    }

    private function findOlderBuild($build=NULL)
    {
        $older = NULL;
        if(!is_null($build) && $build instanceof BuildEvent)
        {
            foreach($this->builds as &$other)
            {
                if($other === $build) continue;

                $otherId = $other->uniqueId();
                if($otherId < $build->uniqueId() &&
                   (is_null($older) || $otherId > $older->uniqueId()))
                {
                    $older = $other;
                }
            }
        }
        return $older;
    }

    private function outputEventList($maxEvents=10)
    {
        if(!isset($this->builds)) return;

        // A clickable button for the RSS feed.
?><div class="buildnews_list"><a href="<?php echo preg_replace('/(&)/', '&amp;', self::RSS_FEED_URI); ?>" class="link-rss" title="Build News via RSS"><span class="hidden">RSS</span></a>&nbsp;<span id="buildnews-label">Latest Build News:</span><?php

        if($this->builds)
        {
            // Generate a table of build events.
?><table><?php

            $n = (integer) 0;
            foreach($this->builds as $event)
            {
                $shortDate = date("d/m/y", $event->startDate());
                $eventHTML = $event->genBadge();

                // Wrap the event in a div which has all if our stylings.
?><tr><td><?php echo $shortDate; ?></td><td><?php echo $eventHTML; ?></td></tr><?php

                if(++$n >= $maxEvents)
                    break;
            }
?></table><?php
        }
        else
        {
?><ul><li><em>...presently unavailable</em></li></ul><?php
        }
?></div><?php
    }

    private function outputPackageList(&$packages, $notThisPack=NULL,
        $chosenPlatformId=NULL, $unstable=-1/*no filter*/, $maxPacks=-1/*no limit*/,
        $listTitleHTML=NULL/*no title*/)
    {
        if(!is_array($packages)) return;

        $maxPacks = intval($maxPacks);
        if($maxPacks === 0) return;
        if($maxPacks < 0) $maxPacks = -1;

        $unstable = intval($unstable);
        if($unstable < 0)
            $unstable = -1; // Any.
        else
            $unstable = $unstable? 1 : 0;

        // Generate package list:
        $numPacks = (integer)0;
        foreach($packages as &$pack)
        {
            // Filtered out?
            if($pack === $notThisPack) continue;
            if($unstable != -1 && (boolean)$unstable != ($pack instanceof AbstractUnstablePackage)) continue;
            if(!is_null($chosenPlatformId) && $pack->platformId() === $chosenPlatformId) continue;

            // Begin the list?
            if(!$numPacks)
            {
                // Compose list title:
?><div class="downloads_list"><?php

                if(!is_null($listTitleHTML))
                {
                    echo $listTitleHTML;
                }

?><ul><?php
            }

            // Generate a pretty download badge/button.
?><li><?php echo $pack->genDownloadBadge(); ?></li><?php

            ++$numPacks;
            if($maxPacks !== -1 && $numPacks === $maxPacks) break;
        }

        if($numPacks)
        {
            // Finish this list.
?></ul></div><?php
        }
    }

    private function outputPackageRedirect(&$pack)
    {
        global $FrontController;

        if(!($pack instanceof AbstractPackage))
            throw new Exception('Received invalid Package.');

        // Begin the page.
        $pageTitle = 'Downloading...';
        $FrontController->outputHeader($pageTitle);
        $FrontController->beginPage($pageTitle);

        // Output the redirect directive.
?><meta http-equiv="REFRESH" content="2;url=<?php echo $pack->downloadUri(); ?>"><?php

        // Generate page content.
?><div id="builds"><?php

?><p>Redirecting to the download for <em><?php echo htmlspecialchars($pack->composeFullTitle()); ?></em>. Your package should begin to download automatically within a few seconds, if not please use this <a href="<?php echo $pack->downloadUri(); ?>" title="<?php echo ('Download '. $pack->composeFullTitle()); ?>">direct link</a> instead.</p><?php

?><p>Not what you wanted? Here are some alternatives:</p><?php

        // Alternative packages:
?><div class="centered"><?php

        // Latest stable packages.
        $packageListTitle = 'Latest packages:';
        $this->outputPackageList(&$this->packages, $pack, PID_ANY, FALSE/*stable*/, 8, $packageListTitle);

        // Latest unstable packages.
        $packageListTitle = 'Latest packages (<a class="link-definition" href="dew/index.php?title=Automated_build_system#Unstable" title="What does \'unstable\' mean?">unstable</a>):';
        $this->outputPackageList(&$this->packages, $pack, PID_ANY, TRUE/*unstable*/, 8, $packageListTitle);

?></div><?php

        // Alternative pages:
        includeHTML('alternativepages', self::$name);

        // End of page content.
?></div><?php

        $FrontController->endPage();
    }

    private function countInstallablePackages(&$build)
    {
        $count = 0;
        if($build instanceof BuildEvent)
        {
            foreach($build->packages as &$pack)
            {
                $downloadUri = $pack->downloadUri();
                if($downloadUri && strlen($downloadUri) > 0)
                {
                    $count++;
                }
            }
        }
        return $count;
    }

    private function genBuildOverview(&$build)
    {
        $html = '';
        if($build instanceof BuildEvent)
        {
            $releaseTypeTexts = array(RT_UNSTABLE=>'unstable', RT_CANDIDATE=>'candidate', RT_STABLE=>'stable');
            $releaseTypeLabel = $releaseTypeTexts[$build->releaseType()];
            $releaseTypeLink = 'dew/index.php?title=Automated_build_system#'.ucfirst($releaseTypeLabel);
            $releaseTypeLinkTitle = "What does '$releaseTypeLabel' mean?";

            $html .= '<h2><a class="link-definition" href="'.$releaseTypeLink.'" title="'.$releaseTypeLinkTitle.'">'. htmlspecialchars(ucfirst($releaseTypeLabel)). '</a></h2>'
                    .'<p>The build event was started on '. date(DATE_RFC2822, $build->startDate()) .'. '
                    .'It contains '. count($build->commits) .' commits and produced '
                    . $this->countInstallablePackages($build) .' installable binary packages.</p>';
        }
        return $html;
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        global $FrontController;

        if(isset($args['getpackage']))
        {
            $this->outputPackageRedirect($args['getpackage']);
            return;
        }

        // Determine whether we are detailing a single build event or listing all events.
        $uniqueId = $args['build'];
        $build = $this->findBuildByUniqueId($uniqueId);

        $pageTitle = (!is_null($build)? $build->composeName() : 'Builds');

        // Output this page.
        $FrontController->outputHeader($pageTitle);
        $FrontController->beginPage($pageTitle);

?><div id="builds"><?php

        if(!is_null($build))
        {
            // Detailing a single build event.

            date_default_timezone_set('EET');

?><div class="buildevent"><?php

            $buildOverview = $this->genBuildOverview($build);
            echo $buildOverview;

            // Use a table.
?><table><tbody><tr><th>OS</th><th>Package</th><th>Logs</th><th>Issues</th></tr><?php

            $packs = $build->packages;
            uasort($packs, array('self', 'packageSorter'));

            $lastPlatId = -1;
            foreach($packs as &$pack)
            {
                $plat = &self::platform($pack->platformId());

                $errors   = $pack->compileErrorCount();
                $warnings = $pack->compileWarnCount();
                $issues   = $errors + $warnings;

                // Determine issue level (think defcon).
                if($errors > 0)
                    $issueLevel = 'major';
                else if($warnings > 0)
                    $issueLevel = 'minor';
                else
                    $issueLevel = 'no';

                // Ouput HTML for the package.
?><tr>
<td><?php if($pack->platformId() !== $lastPlatId) echo $plat['nicename']; ?></td>
<td><a href="<?php echo $pack->downloadUri(); ?>" title="Download <?php echo $pack->composeFullTitle(); ?>"><?php echo $pack->composeFullTitle(true/*include version*/, false/*do not include build Id*/); ?></a></td>
<td><a href="<?php echo $pack->compileLogUri(); ?>" title="Download build logs for <?php echo $pack->composeFullTitle(); ?>">txt.gz</a></td>
<td class="issue_level <?php echo ($issueLevel.'_issue'); ?>"><?php echo $issues; ?></td>
</tr><?php

                $lastPlatId = $pack->platformId();
            }

?></table><?php

            $olderBuild = $this->findOlderBuild($build);
            $newerBuild = $this->findNewerBuild($build);
            $this->outputBuildStreamNavigation($olderBuild, $newerBuild);

            if(count($build->commits))
            {
?><div class="commit_list"><a name="commitindex"></a><h3>Commits</h3>
<script type="text/javascript">
jQuery(document).ready(function() {
  jQuery(".commit").hide();
  jQuery(".collapsible").click(function()
  {
    jQuery(this).next(".commit").slideToggle(300);
  });
});
</script><?php

                outputCommitLog($build);

?></div><?php
            }

?></div><?php

        }
        else
        {
            // Output an overview of the build system.
            includeHTML('overview', self::$name);

?><div class="centered"><?php

            // Generate a navigation widget for the recent build events.
            $this->outputEventList(10/*max events*/);

            // Generate widgets for the symbolic packages.
            $packageListTitle = 'Downloads for the latest packages (<a class="link-definition" href="dew/index.php?title=Automated_build_system#Unstable" title="What does \'unstable\' mean?">unstable</a>):';

            $this->outputPackageList(&$this->symbolicPackages, NULL/*no chosen pack*/,
                PID_ANY, -1/*no stable filter*/, -1/*no result limit*/, $packageListTitle);

?></div><?php

            // Output footnotes for the build system.
            includeHTML('footnotes', self::$name);
        }

?></div><?php

        $FrontController->endPage();
    }
}
