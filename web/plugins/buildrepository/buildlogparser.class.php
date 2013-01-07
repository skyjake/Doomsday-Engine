<?php
/**
 * @file buildlogparser.class.php
 * Parses an autobuilder build event XML file.
 *
 * Constructs a collection of BuildEvents by parsing the XML build log output
 * of the autobuilder script.
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

includeGuard('BuildLogParser');

require_once('buildevent.class.php');
require_once('packagefactory.class.php');
require_once('packages/abstractunstablepackage.class.php');

class BuildLogParser
{
    public static function parse($xmlBuildLog, &$builds)
    {
        if(!is_array($builds))
            throw new Exception('Invalid builds argument, array expected');

        $logDom = self::constructSimpleXmlElementTree($xmlBuildLog);
        if($logDom == FALSE)
            throw new Exception('Failed constructing XML DOM');

        if(!self::parseBuildLogDOM($logDom, $builds))
            throw new Exception('Failed parsing XML DOM');

        return TRUE;
    }

    /**
     * Transform the raw XML data into a SimpleXMLElement node tree.
     *
     * @param $raw_XML  XML data to be parsed (scheme independent).
     * @return  (Object) Root element of a SimpleXMLElement tree if successfull
     *     (Boolean) @c FALSE otherwise.
     */
    private static function constructSimpleXmlElementTree($xmlData)
    {
        libxml_use_internal_errors(TRUE);
        try
        {
            $xmlTree = new SimpleXMLElement($xmlData);
        }
        catch(Exception $e)
        {
            $errorMsg = '';
            foreach(libxml_get_errors() as $libXmlError)
            {
                $errorMsg .= "\t" . $libXmlError->message;
            }
            trigger_error($errorMsg);
            return FALSE;
        }
        return $xmlTree;
    }

    /**
     * Parse the specified build log, constructing from it a collection
     * of BuildEvent objects representing the events.
     *
     * @param log  (Object) Root element of a SimpleXMLElement DOM
     * @param builds  (Array) Builds collection to be populated.
     * @return  (Boolean) @c TRUE iff parse completed successfully.
     */
    private static function parseBuildLogDOM(&$log, &$builds)
    {
        if(!($log instanceof SimpleXMLElement))
            throw new Exception('Received invalid build log');

        // For each build event.
        foreach($log->children() as $log_event)
        {
            try
            {
                $build = self::parseBuildEvent($log_event);

                foreach($log_event->children() as $child)
                {
                    switch($child->getName())
                    {
                    case 'package':
                        try
                        {
                            $pack = PackageFactory::newFromSimpleXMLElement($child, $build->releaseTypeId());
                            $pack->setBuildUniqueId($build->uniqueId());
                            if($pack instanceof iDownloadable)
                            {
                                $pack->setReleaseDate($build->startDate());
                            }
                            if($build->hasReleaseNotesUri())
                            {
                                $pack->setReleaseNotesUri($build->releaseNotesUri());
                            }
                            if($build->hasReleaseChangeLogUri())
                            {
                                $pack->setReleaseChangeLogUri($build->releaseChangeLogUri());
                            }

                            $build->addPackage($pack);
                        }
                        catch(Exception $e)
                        {
                            /// @todo Log exception.
                        }
                        break;

                    case 'commits':
                        $log_commits = $child;
                        foreach($log_commits->children() as $child)
                        {
                            if($child->getName() !== 'commit') continue;

                            try
                            {
                                $commit = self::parseCommit($child);
                                $build->addCommit($commit);
                            }
                            catch(Exception $e)
                            {
                                /// @todo Log exception.
                            }
                        }
                        break;

                    default: break;
                    }
                }

                // Add this new BuildEvent to the collection.
                $builds[] = $build;
            }
            catch(Exception $e)
            {
                /// @todo Log exception.
            }
        }

        return TRUE;
    }

    /**
     * Parse a new Commit record from a SimpleXMLElement.
     *
     * @param $log_commit  (Object) SimpleXMLElement node to be "parsed".
     * @return  (Array) Resultant Commit record.
     */
    private static function parseCommit(&$log_commit)
    {
        if(!($log_commit instanceof SimpleXMLElement))
            throw new Exception('Received invalid log_pack');

        $tags = array();
        foreach($log_commit->children() as $child)
        {
            if($child->getName() !== 'tags') continue;

            foreach($child->children() as $grandchild)
            {
                if($grandchild->getName() !== 'tag') continue;
                $log_tag = $grandchild;

                $tag = clean_text((string)$log_tag);
                $attribs = array();
                foreach($log_tag->attributes() as $key => $value)
                {
                    $attrib = clean_text($key);
                    if(strcasecmp($attrib, 'guessed') == 0)
                    {
                        $value = clean_text($value);
                        $attribs[strtolower($attrib)] = (integer)eval('return ('.$value.');');
                    }
                }

                $tags[$tag] = $attribs;
            }
        }

        $commit = array(
        'submitDate'    => strtotime(clean_text($log_commit->submitDate)),
        'author'        => clean_text($log_commit->author),
        'repositoryUri' => safe_url($log_commit->repositoryUrl),
        'sha1'          => substr(preg_replace('[^a-zA-Z0-9]', '', $log_commit->sha1), 0, 40),
        'title'         => clean_text($log_commit->title),
        'message'       => clean_text($log_commit->message),
        'tags'          => $tags,
        );
        return $commit;
    }

    /**
     * Parse a new BuildEvent from a SimpleXMLElement.
     *
     * @param $log_event  (Object) SimpleXMLElement node to be "parsed".
     * @return  (object) Resultant BuildEvent object.
     */
    private static function parseBuildEvent(&$log_event)
    {
        if(!($log_event instanceof SimpleXMLElement))
            throw new Exception('Received invalid log_event');

        $uniqueId    = (integer)$log_event->uniqueId;
        $startDate   = strtotime(clean_text($log_event->startDate));
        $authorName  = clean_text($log_event->authorName);
        $authorEmail = clean_text($log_event->authorEmail);

        if(!empty($log_event->releaseType))
            $releaseType = BuildRepositoryPlugin::parseReleaseType(clean_text($log_event->releaseType));
        else
            $releaseType = RT_UNSTABLE;

        $event = new BuildEvent($uniqueId, $startDate, $authorName, $authorEmail, $releaseType);
        if(!empty($log_event->releaseNotes))
        {
            $event->setReleaseNotesUri(clean_text($log_event->releaseNotes));
        }
        if(!empty($log_event->changeLog))
        {
            $event->setReleaseChangeLogUri(clean_text($log_event->changeLog));
        }

        return $event;
    }
}
