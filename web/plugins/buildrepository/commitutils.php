<?php
/** @file commitutils.php
 * Utility functions for formatting commit messages
 *
 * @authors Copyright @ 2009-2012 Daniel Swanson <danij@dengine.net>
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

includeGuard('commitutils.php');

require_once(DIR_INCLUDES.'/utilities.inc.php');
require_once(DIR_CLASSES.'/url.class.php');
require_once('buildevent.class.php');

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
    global $FrontController;

    $uri = new Url(implode('', array_slice($matches, 1)));
    $homeUri = new Url($FrontController->homeURL());

    $isExternal = ($uri->host() !== $homeUri->host());
    if(!$isExternal)
    {
        $uri->setScheme()->setHost();
    }

    return generateHyperlinkHTML($uri, 40, $isExternal? 'link-external' : NULL);
}

function formatCommitHTML($msg)
{
    if(strcasecmp(gettype($msg), 'string')) return $msg;

    // Process the commit message, replacing web URIs with clickable links.
    htmlspecialchars($msg);

    $msg = preg_replace_callback('/((?:[\w\d]+\:\/\/)?(?:[\w\-\d]+\.)+[\w\-\d]+(?:\/[\w\-\d]+)*(?:\/|\.[\w\-\d]+)?(?:\?[\w\-\d]+\=[\w\-\d]+\&?)?(?:\#[\w\-\d]*)?)/',
        "make_pretty_hyperlink", $msg);

    return $msg;
}

function formatCommitMessageHTML($msg)
{
    if(strcasecmp(gettype($msg), 'string')) return $msg;

    $msg = formatCommitHTML($msg);
    $msg = nl2br($msg);
    return $msg;
}

function outputCommitHTML(&$commit)
{
    if(!is_array($commit))
        throw new Exception('Invalid commit argument, array expected');

    // Format the commit message for HTML output.
    $title = formatCommitHTML($commit['title']);
    $message = formatCommitMessageHTML($commit['message']);
    $haveMessage = (bool)(strlen($message) > 0);

    // Compose the supplementary tag list.
    $tagList = '<div class="tag_list">';
    if(is_array($commit['tags']))
    {
        $n = (integer)0;
        foreach($commit['tags'] as $tag => $value)
        {
            // Skip the first tag (its used for grouping).
            if($n++ === 0) continue;

            // Do not output guessed tags (mainly used for grouping).
            if(is_array($value) && isset($value['guessed']) && $value['guessed'] !== 0) continue;

            $cleanTag = htmlspecialchars($tag);
            $tagList .= '<div class="tag"><label title="Tagged \''.$cleanTag.'\'">'.$cleanTag.'</label></div>';
        }
    }
    $tagList .= '</div>';

    $repoLinkTitle = 'Show changes in the repository for this commit submitted on '. date(DATE_RFC2822, $commit['submitDate']) .'.';

    // Ouput HTML for the commit.
?><span class="metadata"><a href="<?php echo $commit['repositoryUri']; ?>" class="link-external" title="<?php echo htmlspecialchars($repoLinkTitle); ?>"><?php echo htmlspecialchars(date('Y-m-d', $commit['submitDate'])); ?></a></span><?php

?><p class="heading <?php if($haveMessage) echo 'collapsible'; ?>" <?php if($haveMessage) echo 'title="Toggle commit message display"'; ?>><strong><span class="title"><?php echo $title; ?></span></strong> by <em><?php echo htmlspecialchars($commit['author']); ?></em></p><?php echo $tagList;

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
        $commitCount = count($group);
        $tagLinkTitle = "Jump to commits tagged '$groupName'";

?><li><span class="commit-count"><?php echo htmlspecialchars($commitCount); ?></span><a href="#<?php echo $groupName; ?>" title="<?php echo htmlspecialchars($tagLinkTitle); ?>"><?php echo htmlspecialchars($groupName); ?></a></li><?php
    }
?></ol><?php
}

function outputCommitJumpList(&$groups)
{
    if(!is_array($groups))
        throw new Exception('Invalid groups argument, array expected');

    $groupCount = count($groups);

    // If only one list; apply the special 'hnav' class (force horizontal).
?><div class="jumplist_wrapper <?php if($groupCount <= 5) echo 'hnav'; ?>"><?php

    // If the list is too long; split it into multiple sublists.
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

    $commitCount = count($build->commits);
    if($commitCount)
    {
        $groups = array();
        groupBuildCommits($build, &$groups);

        $groupCount = count($groups);
        if($groupCount > 1)
        {
            ksort($groups);

            // Generate a jump list?
            if($commitCount > 15)
            {
                outputCommitJumpList($groups);
            }
        }

        // Generate the commit list itself.
?><hr />
<div class="commit_list">
<ul><?php

        foreach($groups as $groupName => $group)
        {
?><li><?php

            if($groupCount > 1)
            {
?><strong><label title="<?php echo htmlspecialchars("Commits with primary tag '$groupName'"); ?>"><span class="tag"><?php echo htmlspecialchars($groupName); ?></span></label></strong><a name="<?php echo htmlspecialchars($groupName); ?>"></a><a class="jump" href="#commitindex" title="Back to Commits index">index</a><br /><ol><?php
            }

            foreach($group as &$commit)
            {
?><li <?php if(isset($commit['message']) && strlen($commit['message']) > 0) echo 'class="more"'; ?>><?php

                outputCommitHTML($commit);

?></li><?php
            }

?></ol></li><?php
        }

?></ul></div><?php
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
