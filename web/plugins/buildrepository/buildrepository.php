<?php
/**
 * @file buildrepository.php
 * Build Repository.
 *
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

includeGuard('BuildRepositoryPlugin');

require_once(DIR_CLASSES.'/outputcache.class.php');

/**
 * @defgroup platformId  Platform Identifier
 * @ingroup builds
 */
///@{
define('PID_ANY',                0); ///< (Enumeration). Ordinals are meaningless (can change) however must be unique.
define('PID_WIN_X86',            1);
define('PID_MAC10_4_X86_PPC',    2);
define('PID_MAC10_6_X86_X86_64', 3);
define('PID_LINUX_X86',          4);
define('PID_LINUX_X86_64',       5);
///@}

/**
 * @defgroup releaseType  Release Type
 * @ingroup builds
 */
///@{
define('RT_UNKNOWN',          0); ///< (Enumeration). Ordinals are meaningless (can change) however must be unique.
define('RT_UNSTABLE',         1);
define('RT_CANDIDATE',        2);
define('RT_STABLE',           3);
///@}

require_once('buildevent.class.php');
require_once('buildlogparser.class.php');
require_once('commitutils.php');
require_once('packagefactory.class.php');

class BuildRepositoryPlugin extends Plugin implements Actioner, RequestInterpreter
{
    /// Plugin name.
    public static $name = 'buildrepository';

    /// Feed URIs:
    const XML_FEED_URI = 'http://dl.dropbox.com/u/11948701/builds/events.xml';
    const RSS_FEED_URI = 'http://dl.dropbox.com/u/11948701/builds/events.rss';

    /**
     * Plaform (Record):
     *
     * 'id': @see platformId
     * 'name': Symbolic name used to identify this platform.
     * 'nicename': User facing/friendly name for this platform.
     */
    private static $platforms = array(
        PID_WIN_X86            => array('id'=>PID_WIN_X86,            'name'=>'win-x86',            'nicename'=>'Windows'),
        PID_MAC10_4_X86_PPC    => array('id'=>PID_MAC10_4_X86_PPC,    'name'=>'mac10_4-x86-ppc',    'nicename'=>'Mac OS (10.4+)'),
        PID_MAC10_6_X86_X86_64 => array('id'=>PID_MAC10_6_X86_X86_64, 'name'=>'mac10_6-x86-x86_64', 'nicename'=>'Mac OS (10.6+)'),
        PID_LINUX_X86          => array('id'=>PID_LINUX_X86,          'name'=>'linux-x86',          'nicename'=>'Ubuntu (32bit)'),
        PID_LINUX_X86_64       => array('id'=>PID_LINUX_X86_64,       'name'=>'linux-x86_64',       'nicename'=>'Ubuntu (64bit)')
    );
    private static $unknownPlatform = array(
        'id'=>PID_ANY, 'name'=>'unknown', 'nicename'=>'Unknown');

    /**
     * ReleaseType (Record):
     *
     * 'id': @see releaseTypeId
     * 'name': Symbolic name used to identify this release type.
     * 'nicename': User facing/friendly name for this release type.
     */
    private static $releaseTypes = array(
        RT_UNSTABLE  => array('id'=>RT_UNSTABLE,    'name'=>'unstable',     'nicename'=>'Unstable'),
        RT_CANDIDATE => array('id'=>RT_CANDIDATE,   'name'=>'candidate',    'nicename'=>'Candidate'),
        RT_STABLE    => array('id'=>RT_STABLE,      'name'=>'stable',       'nicename'=>'Stable')
    );
    private static $unknownReleaseType = array(
        'id'=>RT_UNKNOWN, 'name'=>'unknown', 'nicename'=>'Unknown');

    /// Special 'null' package.
    private static $nullPack = NULL;

    /// Builds.
    private $builds;

    /// Package collections.
    private $packages = NULL;
    private $symbolicPackages = NULL;

    /// Weight values for sorting of Packages.
    private static $packageWeights = array(
        'DistributionPackage'                =>0,
        'DistributionBuilderPackage'         =>0,
        'PluginPackage'                      =>1,
        'PluginBuilderPackage'               =>1,
        'DistributionUnstablePackage'        =>2,
        'DistributionUnstableBuilderPackage' =>2,
        'PluginUnstablePackage'              =>3,
        'PluginUnstableBuilderPackage'       =>3
    );

    /// @return  Plugin name.
    public function title()
    {
        return self::$name;
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
        // Unknown.
        return RT_UNKNOWN;
    }

    /**
     * Retrieve the ReleaseType record associated with @a releaseTypeId.
     * If no match is found then the special 'unknown' release is returned.
     *
     * @param releaseTypeId  (Integer) Unique identfier of the release type.
     * @return  (Object) Determined ReleaseType.
     */
    public static function &releaseType($releaseTypeId=RT_UNKNOWN)
    {
        $releaseTypeId = intval($releaseTypeId);
        if(isset(self::$releaseTypes[$releaseTypeId])) return self::$releaseTypes[$releaseTypeId];
        return self::$unknownReleaseType;
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
        if($a !== $b) return $a < $b? -1 : 1;

        // Lastly by package title.
        return strcasecmp($packA->title(), $packB->title());
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
    private static function mustUpdateCachedBuildLog(&$buildLogUri, &$cacheName)
    {
        global $FrontController;

        if(!$FrontController->contentCache()->isPresent($cacheName))
            return TRUE;

        // Only query the remote server at most once every five minutes for an
        // updated build log. The modified time of our local cached copy is used
        // to determine when it is time to query (we touch the cached copy after
        // each query attempt).
        $cacheInfo = new ContentInfo();
        $FrontController->contentCache()->getInfo($cacheName, $cacheInfo);
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
            /// @todo Store error so users can query.
            //setError($e->getMessage());
            return FALSE;
        }
    }

    private static function retrieveBuildLogXml(&$buildLogUri)
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

    /**
     * Attempt to parse the build log, constructing from it a collection
     * of the abstract objects we use to model the events an packages it
     * defines.
     *
     * @param builds  (Array) Collection to be populated with BuildEvents.
     * @return (Boolean) @c TRUE iff successful.
     */
    private function constructBuilds(&$builds)
    {
        global $FrontController;

        $buildLogUri = self::XML_FEED_URI;

        // Is it time to update our cached copy of the build log?
        $logCacheName = 'buildrepository/events.xml';
        if(self::mustUpdateCachedBuildLog($buildLogUri, $logCacheName))
        {
            try
            {
                // Grab a copy and store it in the local file cache.
                $logXml = self::retrieveBuildLogXml($buildLogUri);
                if($logXml == FALSE)
                    throw new Exception('Failed retrieving build log');

                // Attempt to parse the new log.
                BuildLogParser::parse($logXml, $builds);

                // Parsed successfully; update the cache with this new file.
                $FrontController->contentCache()->store($logCacheName, $logXml);
                return TRUE;
            }
            catch(Exception $e)
            {
                // Free up resources.
                unset($logXml);

                // Log the error.
                trigger_error(sprintf('Failed parsing new XML build log.\nError:%s', $e->getMessage()), E_USER_WARNING);

                // Touch our cached copy so we don't try again too soon.
                $FrontController->contentCache()->touch($logCacheName);
            }
        }

        // Re-parse our locally cached copy of the log, hopefully
        // we don't need to do this too often (cache everything!).
        try
        {
            $cachedLogXml = $FrontController->contentCache()->retrieve($logCacheName);
            BuildLogParser::parse($cachedLogXml, $builds);
            return TRUE;
        }
        catch(Exception $e)
        {
            // Yikes! Looks like we broke something...
            trigger_error(sprintf('Failed parsing cached XML build log.\nError:%s', $e->getMessage()), E_USER_WARNING);
            return FALSE;
        }
    }

    private function grabAndParseBuildFeedXML()
    {
        $builds = array();
        $this->constructBuilds($builds);
        $this->builds = $builds;
    }

    /**
     * Populate the collection with 'static' packages. A static package is
     * one which was not produced by or that which has no persistent build
     * event information.
     *
     * Static packages are primarily for historic releases which predate
     * the autobuilder.
     *
     * @param packages  (Array) Collection to be populated.
     * @return  (Boolean) @c TRUE iff successful.
     */
    private function populateStaticPackages(&$packages)
    {
        /**
         * @todo Read this information from a config file, we should not
         * expect to edit this file in order to change these...
         */

        $pack = PackageFactory::newDistribution(PID_WIN_X86, 'Doomsday', '1.8.6',
                                                'http://sourceforge.net/projects/deng/files/Doomsday%20Engine/1.8.6/deng-inst-1.8.6.exe/download',
                                                false/*not an autobuilder packaged*/);
        $pack->setReleaseNotesUri('http://dengine.net/dew/index.php?title=Doomsday_version_1.8.6');
        $packages[] = $pack;

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
         * @todo Read this information from a config file, we should not
         * expect to edit this file in order to change these...
         */

        // Windows:
        $plat = $this->platform(PID_WIN_X86);
        $pack = PackageFactory::newDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']);
        $packages[] = $pack;

        $plat = $this->platform(PID_WIN_X86);
        $pack = PackageFactory::newDistributionUnstable($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        // Mac OS:
        $plat = $this->platform(PID_MAC10_4_X86_PPC);
        $pack = PackageFactory::newDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']);
        $packages[] = $pack;

        $plat = $this->platform(PID_MAC10_4_X86_PPC);
        $pack = PackageFactory::newDistributionUnstable($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        $plat = $this->platform(PID_MAC10_6_X86_X86_64);
        $pack = PackageFactory::newDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']);
        $packages[] = $pack;

        $plat = $this->platform(PID_MAC10_6_X86_X86_64);
        $pack = PackageFactory::newDistributionUnstable($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        // Ubuntu:
        $plat = $this->platform(PID_LINUX_X86);
        $pack = PackageFactory::newDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']);
        $packages[] = $pack;

        $plat = $this->platform(PID_LINUX_X86);
        $pack = PackageFactory::newDistributionUnstable($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        $plat = $this->platform(PID_LINUX_X86_64);
        $pack = PackageFactory::newDistribution($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']);
        $packages[] = $pack;

        $plat = $this->platform(PID_LINUX_X86_64);
        $pack = PackageFactory::newDistributionUnstable($plat['id'], 'Latest Doomsday',
            NULL/*no version*/, 'latestbuild?platform='. $plat['name']. '&unstable');
        $packages[] = $pack;

        return TRUE;
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

    /**
     * Chain BuildEvents together by 'start date', linking them according
     * to their start timestamps.
     */
    private function chainBuildsByStartDate()
    {
        if(!isset($this->builds)) return FALSE;

        foreach($this->builds as &$build)
        {
            $older = $this->findOlderBuild($build);
            $build->setPrevForStartDate($older);

            $newer = $this->findNewerBuild($build);
            $build->setNextForStartDate($newer);
        }

        return TRUE;
    }

    /**
     * Chain BuildEvents together by 'version', linking them according
     * to the version number of the 'Doomsday' package(s) those events
     * produced.
     */
    private function chainBuildsByDoomsdayVersion()
    {
        if(!isset($this->packages)) return FALSE;

        $releases = array();
        foreach($this->packages as &$pack)
        {
            // We are only interested in the 'Doomsday' packages.
            if($pack->title() !== 'Doomsday') continue;

            // Have we encountered this version before?.
            $version = $pack->version();
            $key = array_casekey_exists($version, $releases);
            if($key === false)
            {
                // Not yet construct a new build list (array) and associate it
                // in the release array using the version number as the key.
                $key = ucwords($version);
                $releases[$key] = array();

                $buildList = &$releases[$key];
            }
            else
            {
                $buildList = &$releases[$version];
            }

            // Is this package a product of the autobuilder?
            if($pack instanceof iBuilderProduct)
            {
                // Yes; we have "real" BuildEvent we can link with this.
                $buildUniqueId = $pack->buildUniqueId();
                $build = $this->buildByUniqueId($buildUniqueId);
            }
            else
            {
                // No - this must be a symbolic package.
                // We'll instantiate a symbolic BuildEvent for this.
                $build = new BuildEvent(0, strtotime('Jan 8, 2005'), 'skyjake',
                                        'jaakko.keranen@iki.fi', RT_STABLE/*assumed*/);
                if($pack->hasReleaseNotesUri())
                {
                    $build->setReleaseNotesUri($pack->releaseNotesUri());
                }
                if($pack->hasReleaseChangeLogUri())
                {
                    $build->setReleaseChangeLogUri($pack->releaseChangeLogUri());
                }
                $build->addPackage($pack);
            }

            if(!$build instanceof BuildEvent) continue; // Odd...

            // Is this build event already present in the index for this release version?
            if(array_search($build, $buildList) !== FALSE) continue;

            // Add this build to the index.
            $buildList[] = $build;
        }

        foreach($releases as $version => &$buildList)
        {
            $numEvents = count($buildList);
            for($i = (integer)0; $i < $numEvents; $i++)
            {
                $curEvent = &$buildList[$i];
                if(!$curEvent instanceof BuildEvent) throw new Exception('BuildEvent expected but received something else.');

                $prevEvent = (($i < $numEvents && isset($buildList[$i+1]))? $buildList[$i+1] : -1);
                if($prevEvent instanceof BuildEvent)
                {
                    $curEvent->setPrevForVersion($prevEvent);
                }

                $nextEvent = (($i > 0 && isset($buildList[$i-1]))? $buildList[$i-1] : -1);
                if($nextEvent instanceof BuildEvent)
                {
                    $curEvent->setNextForVersion($nextEvent);
                }
            }
        }

        return TRUE;
    }

    /**
     * Chain BuildEvents together to form a navigable web of events.
     */
    private function chainBuilds()
    {
        $this->chainBuildsByStartDate();
        $this->chainBuildsByDoomsdayVersion();
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

        // Link build events together to form the navigation chains.
        $this->chainBuilds();
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
     * @param hasDownload  (Boolean) @c true= Only consider 'downloadable' packs.
     * @return  (Object) Chosen package.
     */
    private function &choosePackage($platformId=PID_ANY, $title="Doomsday",
        $unstable=FALSE, $downloadable=TRUE)
    {
        $unstable = (boolean)$unstable;
        $downloadable = (boolean)$downloadable;

        if(isset($this->packages))
        {
            $matchTitle = (boolean)(strlen($title) > 0);

            foreach($this->packages as &$pack)
            {
                if($pack->platformId() !== $platformId) continue;
                if($matchTitle && strcasecmp($pack->title(), $title)) continue;
                if($unstable != ($pack instanceof AbstractUnstablePackage)) continue;
                if($downloadable != ($pack instanceof iDownloadable && $pack->hasDirectDownloadUri())) continue;

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

            // Are we redirecting to a specific package?
            $getPackage = FALSE;
            if(isset($uriArgs['platform']))
            {
                // Parse and validate arguments.
                $platformId = $this->parsePlatformId($uriArgs['platform']);
                $unstable = isset($uriArgs['unstable']);

                // Default to Doomsday if a pack is not specified.
                $packTitle = "Doomsday";
                if(isset($uriArgs['pack']))
                {
                    $packTitle = trim($uriArgs['pack']);
                }

                // Try to find a suitable package...
                $pack = &$this->choosePackage($platformId, $packTitle, $unstable);
                if(!($pack instanceof NullPackage))
                {
                    $args = array();

                    // Are we retrieving the object graph?
                    if(isset($uriArgs['graph']))
                    {
                        // Return the object graph.
                        $args['getgraph'] = $pack;
                    }
                    else
                    {
                        // Redirect to the package download.
                        $args['getpackage'] = $pack;
                    }

                    $FrontController->enqueueAction($this, $args);
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

            $buildId = intval(substr($tokens[0], 5));

            // Is this a known event?
            $build = $this->buildByUniqueId($buildId);
            if(!$build instanceof BuildEvent)
                $buildId = 0; // Show the index.

            $FrontController->enqueueAction($this, array('build' => $buildId));
            return true; // Eat the request.
        }

        return false; // Not for us.
    }

    /**
     * Output build event stream navigational controls.
     *
     * @param currentEvent (integer) Current event.
     */
    private function outputBuildStreamNavigation(&$event)
    {
        $headEvent = $event->prevForStartDate();
        if(!$headEvent instanceof BuildEvent)
            $headEvent = $event;

?><div id="buildsnav" class="hnav"><h3><span>&larr;Older</span> <a href="builds" title="Back to the Build Repository index">Index</a> <span>Newer&rarr;</span></h3><?php
?><div class="buildstreamlist"><?php

        $this->outputBuildStreamWidget($headEvent, 'startdate', TRUE/*ascend*/, 3,
                                       NULL/*no release header*/,
                                       TRUE/*use the horiztonal variant*/,
                                       $event, TRUE/*current is inactive*/);

?></div></div><?php

    }

    /**
     * Retrieve the BuildEvent object associated with @a uniqueId.
     *
     * @param uniqueId  (Integer) Unique identfier of the build event.
     * @return  (Mixed) BuildEvent object for the build else @c NULL.
     */
    public function buildByUniqueId($uniqueId=0)
    {
        $uniqueId = intval($uniqueId);
        if($uniqueId > 0)
        foreach($this->builds as &$build)
        {
            if($uniqueId === $build->uniqueId()) return $build;
        }
        return NULL;
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
?><tr><td><?php echo htmlspecialchars($shortDate); ?></td><td><?php echo $eventHTML; ?></td></tr><?php

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
        $chosenPlatformId=NULL, $unstable=-1/*no filter*/, $downloadable=-1/*no filter*/,
        $maxPacks=-1/*no limit*/, $listTitleHTML=NULL/*no title*/)
    {
        if(!is_array($packages)) return;

        $maxPacks = intval($maxPacks);
        if($maxPacks === 0) return;
        if($maxPacks < 0) $maxPacks = -1;

        $unstable = intval($unstable);
        if($unstable < 0) $unstable = -1; // Any.
        else              $unstable = $unstable? 1 : 0;

        $downloadable = intval($downloadable);
        if($downloadable < 0) $downloadable = -1; // Any.
        else                  $downloadable = $downloadable? 1 : 0;

        // Generate package list:
        $numPacks = (integer)0;
        foreach($packages as &$pack)
        {
            // Filtered out?
            if($pack === $notThisPack) continue;
            if($unstable != -1 && (boolean)$unstable == ($pack instanceof AbstractUnstablePackage)) continue;
            if($downloadable != -1 && (boolean)$downloadable != ($pack instanceof iDownloadable && $pack->hasDirectDownloadUri())) continue;
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
?><meta http-equiv="REFRESH" content="2;url=<?php echo $pack->directDownloadUri(); ?>"><?php

        // Generate page content.
?><div id="builds"><?php

?><p>Redirecting to the download for <em><?php echo htmlspecialchars($pack->composeFullTitle()); ?></em>. Your package should begin to download automatically within a few seconds, if not please use this <a href="<?php echo $pack->directDownloadUri(); ?>" title="<?php echo htmlspecialchars('Download '. $pack->composeFullTitle()); ?>">direct link</a> instead.</p><?php

?><p>Not what you wanted? Here are some alternatives:</p><?php

        // Alternative packages:
?><div class="centered"><?php

        // Latest stable packages.
        $packageListTitle = 'Latest packages:';
        $this->outputPackageList(&$this->packages, $pack, PID_ANY, FALSE/*stable*/,
                                 TRUE/*only downloadables*/, 8, $packageListTitle);

        // Latest unstable packages.
        $packageListTitle = 'Latest packages (<a class="link-definition" href="dew/index.php?title=Automated_build_system#Unstable" title="What does \'unstable\' mean?">unstable</a>):';
        $this->outputPackageList(&$this->packages, $pack, PID_ANY, TRUE/*unstable*/,
                                 TRUE/*only downloadables*/, 8, $packageListTitle);

?></div><?php

        // Alternative pages:
        includeHTML('alternativepages', self::$name);

        // End of page content.
?></div><?php

        $FrontController->endPage();
    }

    /**
     * Compose the cache name for the Package's object graph.
     * Packages produced by a build event go into that build's subdirectory.
     * Other packages (i.e., stable or symbolic) are placed in the root.
     */
    private function composePackageGraphCacheName(&$pack)
    {
        if(!($pack instanceof BasePackage))
            throw new Exception('Received invalid Package.');

        $cacheName = 'buildrepository/';
        if($pack instanceof iBuilderProduct)
        {
            $cacheName .= $pack->buildUniqueId().'/';
        }
        $cacheName .= md5(strtolower($pack->composeFullTitle())).'.json';

        return $cacheName;
    }

    private function outputPackageGraph(&$pack)
    {
        global $FrontController;

        if(!($pack instanceof BasePackage))
            throw new Exception('Received invalid Package.');

        $cacheName = $this->composePackageGraphCacheName($pack);
        try
        {
            if(!$FrontController->contentCache()->isPresent($cacheName))
            {
                // Generate a graph template for this package.
                $template = array();
                $pack->populateGraphTemplate($template);
                $json = json_encode_clean($template);

                // Store the graph in the cache.
                $FrontController->contentCache()->store($cacheName, $json);
            }

            $contentInfo = new ContentInfo();
            if($FrontController->contentCache()->getInfo($cacheName, $contentInfo))
            {
                header('Pragma: public');
                header('Cache-Control: public');
                header('Content-Type: application/json');
                header('Last-Modified: '. date(DATE_RFC1123, $contentInfo->modifiedTime));
                header('Expires: '. date(DATE_RFC1123, strtotime('+5 days')));

                $FrontController->contentCache()->import($cacheName);
            }
        }
        catch(Exception $e)
        {
            // Log the error.
            trigger_error(sprintf('Failed reading Package JSON from cache.\nError:%s', $e->getMessage()), E_USER_WARNING);
        }
        return TRUE;
    }

    private function countInstallablePackages(&$build)
    {
        $count = 0;
        if($build instanceof BuildEvent)
        {
            foreach($build->packages as &$pack)
            {
                if($pack instanceof iDownloadable && $pack->hasDirectDownloadUri())
                {
                    $count++;
                }
            }
        }
        return $count;
    }

    private function outputBuildEventMetadata(&$build)
    {
        if(!$build instanceof BuildEvent) return;

        $releaseType = self::releaseType($build->releaseTypeId());

        $releaseTypeLabel = $releaseType['nicename'];
        $releaseTypeLink = 'dew/index.php?title=Automated_build_system#'. $releaseTypeLabel;
        $releaseTypeLinkTitle = "What does '$releaseTypeLabel' mean?";

        $buildNumberLabel = $build->uniqueId();
        $buildNumberLink = 'dew/index.php?title=Build_number#Build_numbers';
        $buildNumberLinkTitle = "What does 'Build$buildNumberLabel' mean?";

?><table id="buildeventmetadata">
<tbody>
<tr><th colspan="2">Event</th></tr>
<tr><td>Start date </td><td><?php echo htmlspecialchars(date(/*DATE_RFC850*/ "d-M-Y", $build->startDate())); ?></td></tr>
<tr><td>Start time </td><td><?php echo htmlspecialchars(date(/*DATE_RFC850*/ "H:i:s T", $build->startDate())); ?></td></tr>
<tr><td>Release type </td><td><a class="link-definition" href="<?php echo $releaseTypeLink; ?>" title="<?php echo htmlspecialchars($releaseTypeLinkTitle); ?>"><?php echo htmlspecialchars($releaseTypeLabel); ?></a></td></tr>
<tr><td>Build number </td><td><a class="link-definition" href="<?php echo $buildNumberLink; ?>" title="<?php echo htmlspecialchars($buildNumberLinkTitle); ?>"><?php echo htmlspecialchars(ucfirst($buildNumberLabel)); ?></a></td></tr><?php

        $installablesCount = $this->countInstallablePackages($build);
        if($installablesCount > 0)
        {

?><tr><td>Packages </td><td><?php echo htmlspecialchars($installablesCount); ?></td></tr><?php

        }

?></tbody></table><?php
    }

    private function outputBuildPackageList(&$build)
    {
        if(!$build instanceof BuildEvent) return;

        // Use a table.
?><table><tbody><tr><th>OS</th><th>Package</th><th>Logs</th><th>Issues</th></tr><?php

        $packs = $build->packages;
        uasort($packs, array('self', 'packageSorter'));

        $lastPlatId = -1;
        foreach($packs as &$pack)
        {
            $plat = &self::platform($pack->platformId());

            if($pack instanceof AbstractPackage)
            {
                $errors   = $pack->compileErrorCount();
                $warnings = $pack->compileWarnCount();
                $issues   = $errors + $warnings;
            }
            else
            {
                $errors = 0;
                $warnings = 0;
                $issues = 0;
            }

            // Determine issue level (think defcon).
            if($errors > 0 || !$pack->hasDirectDownloadUri())
            {
                $issueLevel = 'major';
                $issueTooltip = "$errors major errors occurred during the build";
            }
            else if($warnings > 0)
            {
                $issueLevel = 'minor';
                $issueTooltip = "$warnings warnings occurred during the build";
            }
            else
            {
                $issueLevel = 'no';
                $issueTooltip = 'Build completed without issue';
            }

            // Ouput HTML for the package.
?><tr>
<td><?php if($pack->platformId() !== $lastPlatId) echo htmlspecialchars($plat['nicename']); ?></td>
<td><?php

            $packTitle = $pack->composeFullTitle(true/*include version*/, false/*do not include the platform name*/, false/*do not include build Id*/);
            if($pack instanceof iDownloadable && $pack->hasDirectDownloadUri())
            {
?><a href="<?php echo $pack->directDownloadUri(); ?>" title="Download <?php echo htmlspecialchars($pack->composeFullTitle()); ?>"><?php echo htmlspecialchars($packTitle); ?></a><?php
            }
            else
            {
                echo htmlspecialchars($packTitle);
            }

?></td><td><?php

            if($pack instanceof AbstractPackage)
            {
                $logUri = $pack->compileLogUri();

?><a href="<?php echo $logUri; ?>" title="Download build logs for <?php echo htmlspecialchars($pack->composeFullTitle()); ?>">txt.gz</a><?php

            }
            else
            {
?>txt.gz<?php
            }

?></td><td><div class="issue_level <?php echo htmlspecialchars($issueLevel.'_issue'); ?>" title="<?php echo htmlspecialchars($issueTooltip); ?>"><?php echo htmlspecialchars($issues); ?></div></td>
</tr><?php

            $lastPlatId = $pack->platformId();
        }

?></table><?php
    }

    private function outputBuildCommitLog(&$build)
    {
        if(!$build instanceof BuildEvent) return;

        if(count($build->commits))
        {
?><div id="buildcommits"><a name="commitindex"></a><h3><?php echo count($build->commits); ?> Commits</h3>
<script type="text/javascript">
jQuery(document).ready(function() {
  jQuery(".commit").hide();
  jQuery(".collapsible").click(function()
  {
    jQuery(this).next().next(".commit").slideToggle(300);
  });
});
</script><?php

                outputCommitLog($build);

?></div><?php
        }
    }

    /**
     * Print an HTML representation of the detailed information we have for
     * the specified build @a event to the output stream.
     *
     * @param event  (object) BuildEvent to be detailed.
     */
    private function outputEventDetail(&$event)
    {
        if(!$event instanceof BuildEvent) throw new Exception('outputEventDetail: Invalid build argument, BuildEvent expected');

?><div class="buildevent"><?php

        // Display an overview of the event.
?><div id="buildoverview"><?php

        $this->outputBuildEventMetadata($event);
        $this->outputBuildPackageList($event);

?></div><?php

        // Display a stream navigation widget.
        $this->outputBuildStreamNavigation($event);

        // Display the full commit log.
        $this->outputBuildCommitLog($event);

?></div><?php
    }

    /**
     * Construct a new ReleaseInfo record (array).
     *
     * Properties:
     *
     *   version       < (string) Version string of the Doomsday package.
     *   releaseTypeId < (integer) @ref releaseType
     *   latestBuild   < (BuildEvent) Latest build event for this logical release.
     */
    private function newReleaseInfo($version)
    {
        $record = array();
        $record['version'] = strval($version);
        $record['releaseTypeId'] = RT_UNKNOWN; // Default.
        $record['latestBuild'] = NULL;
        return $record;
    }

    /**
     * Produce a indexable directory of BuildEvents from the Packages
     * collection, grouping them according to the version number of the
     * 'Doomsday' packages those events produced.
     *
     * The version number (string) is used as key to each record.
     *
     * @param matrix  (Array) will be populated with new records.
     * @return  (Mixed) FALSE if no packages were added to the matrix
     *          otherwise the number of added packages (integer).
     */
    private function populateReleases(&$releases)
    {
        if(!is_array($releases)) throw new Exception('populateReleases: Invalid matrix argument, array expected.');
        if(!isset($this->packages)) return FALSE;

        // Running total of the number of events we add to the matrix.
        $numEventsAdded = (integer)0;

        foreach($this->packages as &$pack)
        {
            // We are only interested in the 'Doomsday' packages.
            if($pack->title() !== 'Doomsday') continue;

            // Have we encountered this version before?.
            $version = $pack->version();
            $key = array_casekey_exists($version, $releases);
            if($key === false)
            {
                // Not yet construct a new record and associate it
                // in the release list using the version number as the key.
                $key = ucwords($version);
                $releases[$key] = $this->newReleaseInfo($key);

                $releaseInfo = &$releases[$key];
            }
            else
            {
                $releaseInfo = &$releases[$version];
            }

            $build = NULL;

            // Is this package a product of the autobuilder?
            if($pack instanceof iBuilderProduct)
            {
                // Yes; we have "real" BuildEvent we can link with this.
                $buildUniqueId = $pack->buildUniqueId();
                $build = $this->buildByUniqueId($buildUniqueId);
            }
            else
            {
                // No - this must be a symbolic package.
                // We'll instantiate a symbolic BuildEvent for this.
                $build = new BuildEvent(0, strtotime('Jan 8, 2005'), 'skyjake',
                                        'jaakko.keranen@iki.fi', RT_STABLE/*assumed*/);
                if($pack->hasReleaseNotesUri())
                {
                    $build->setReleaseNotesUri($pack->releaseNotesUri());
                }
                if($pack->hasReleaseChangeLogUri())
                {
                    $build->setReleaseChangeLogUri($pack->releaseChangeLogUri());
                }
                $build->addPackage($pack);
            }

            if(!$build instanceof BuildEvent) continue; // Odd...

            // Is a build event already present for this release version?
            $latestBuild = (isset($releaseInfo['latestBuild']) ? $releaseInfo['latestBuild'] : NULL);
            if($latestBuild instanceof BuildEvent)
            {
                // Is this a newer build?
                if($build->uniqueId() > $latestBuild->uniqueId())
                {
                    $releaseInfo['latestBuild'] = $build;
                }
            }
            else
            {
                $releaseInfo['latestBuild'] = $build;
            }

            // Promote the status of the release due to this package?
            $releaseTypeId = $build->releaseTypeId();
            if($releaseTypeId > $releaseInfo['releaseTypeId'])
            {
                $releaseInfo['releaseTypeId'] = $releaseTypeId;
            }
        }

        return $numEventsAdded;
    }

    private function outputBuildStreamWidget(&$headEvent, $chainProperty='version',
        $chainDirection=FALSE, $chainLengthMax=-1, $releaseInfo=NULL, $horizontal=FALSE,
        $currentEvent=NULL, $currentInactive=FALSE)
    {
        $chainDirection = (boolean)$chainDirection;
        $chainLengthMax = (integer)$chainLengthMax;
        $currentInactive = (boolean)$currentInactive;

?><div class="buildstream<?php echo ($horizontal ? ' hnav' : ''); ?>"><ul><?php

        // Include a release version header?
        if(is_array($releaseInfo))
        {
            $releaseTypeId = $releaseInfo['releaseTypeId'];
            $version = $releaseInfo['version'];
            $releaseType = $this->releaseType($releaseTypeId);
            $releaseLabel = htmlspecialchars($version);

            // See if the latest package includes a release notes URI
            // if it does we'll add a link to it to the column title.
            $latestBuild = $releaseInfo['latestBuild'];
            if($latestBuild instanceof BuildEvent && $latestBuild->hasReleaseNotesUri())
            {
                $releaseTypeLink = $latestBuild->releaseNotesUri();
                $releaseTypeLinkTitle = htmlspecialchars("Read the release notes for {$version}");

                $releaseLabel = "<a href=\"{$releaseTypeLink}\" title=\"{$releaseTypeLinkTitle}\">{$releaseLabel}</a>";
            }

?><li><div class="release-badge"><?php echo $releaseLabel; ?></div></li><?php

        }

        if($headEvent instanceof BuildEvent)
        {
            $n = (integer) 0;
            for($event = $headEvent; !is_null($event);
                $event = $chainProperty === 'version' ? ($chainDirection? $event->nextForVersion()   : $event->prevForVersion())
                                                      : ($chainDirection? $event->nextForStartDate() : $event->prevForStartDate()))
            {
                $cssClass = '';
                if($currentEvent instanceof BuildEvent && $event === $currentEvent)
                {
                    $cssClass = ' class="current"';
                }

                $isActive = !($currentInactive && ($currentEvent instanceof BuildEvent && $event === $currentEvent));

?><li<?php echo $cssClass; ?>><?php

                echo $event->genFancyBadge($isActive);

?></li><?php

                $n++;
                if($chainLengthMax > 0 && $n >= $chainLengthMax) break;
            }

            if($chainLengthMax > 0)
            {
                while($n++ < $chainLengthMax)
                {

?><li></li><?php

                }
            }
        }
        else
        {

?><li></li><?php

        }

?></ul></div><?php
    }

    /**
     * Print an HTML representation of the supplied build event matrix
     * to the output stream.
     */
    private function outputEventMatrix(&$releases)
    {
        if(!is_array($releases)) throw new Exception('outputEventMatrix: Invalid releases argument, array expected.');

?><div class="buildstreamlist"><?php

        foreach($releases as &$releaseInfo)
        {
            $event = isset($releaseInfo['latestBuild']) ? $releaseInfo['latestBuild'] : -1;

            $current = $event;

            $this->outputBuildStreamWidget($event, 'version', FALSE/*descend*/, -1/*no length limit*/,
                                           $releaseInfo, FALSE/*vertical*/, $current, FALSE);
        }

?></div><?php

    }

    /**
     * Merge the BuildEvent matrix to the output stream.
     */
    private function includeEventMatrix()
    {
        // Construct the release list.
        $releases = array();
        $this->populateReleases($releases);

        // Sort by key to achieve the version-number-ascending order.
        ksort($releases);
        //print_r($releases);

        $this->outputEventMatrix($releases);
    }

    /**
     * Print an HTML representation of the entire builds collection to
     * the output stream.
     */
    private function outputEventIndex()
    {
?><div class="buildevents_outer"><table><thead><tr><th></th><th><?php

?>Version<?php

?></th></tr></thead><?php

?><tbody><tr><td><?php

        // Output stream info
        includeHTML('streaminfo', self::$name);

?></td><td><?php

        $this->includeEventMatrix();

?></td></tr></tbody></table></div><?php

?><div class="buildsoverview"><span id="roadmap_badge"><a href="dew/index.php?title=Roadmap" title="Read the Roadmap at the Wiki">Roadmap</a></span><?php

        // Output an overview of the build system.
        includeHTML('overview', self::$name);

        // Generate widgets for the symbolic packages.
        $packageListTitle = '<h3>Downloads for the latest packages</h3>';

        $this->outputPackageList(&$this->symbolicPackages, NULL/*no chosen pack*/, PID_ANY,
                                 TRUE/*stable filter*/, TRUE/*only downloadables*/,
                                 -1/*no result limit*/, $packageListTitle);

        $packageListTitle = '<h3>Downloads for the latest packages (<a class="link-definition" href="dew/index.php?title=Automated_build_system#Unstable" title="What does \'unstable\' mean?">unstable</a>)</h3>';

        $this->outputPackageList(&$this->symbolicPackages, NULL/*no chosen pack*/, PID_ANY,
                                 FALSE/*no stable filter*/, TRUE/*only downloadables*/,
                                 -1/*no result limit*/, $packageListTitle);

?></div><?php
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        global $FrontController;

        // The autobuilder operates in Eastern European Time.
        date_default_timezone_set('EET');

        if(isset($args['getpackage']))
        {
            $this->outputPackageRedirect($args['getpackage']);
            return;
        }
        else if(isset($args['getgraph']))
        {
            $this->outputPackageGraph($args['getgraph']);
            return;
        }

        // Determine whether we are detailing a single build event or listing all events.
        $build = isset($args['build'])? $this->buildByUniqueId($args['build']) : NULL;

        $pageTitle = (($build instanceof BuildEvent)? $build->composeName(true/*add release type*/) : 'Build Repository');

        // Output this page.
        $FrontController->outputHeader($pageTitle);
        $FrontController->beginPage($pageTitle);

?><div id="builds"><?php

        if($build instanceof BuildEvent)
        {
            // Detailing a single build event.
            $this->outputEventDetail($build);
        }
        else
        {
            // Show the entire build event index.
            $this->outputEventIndex();
        }

?></div><?php

        $FrontController->endPage();
    }
}
