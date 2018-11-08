<?php
    require_once('class.sitedata.php');
    require_once('utils.inc.php');
    
    // Check the latest build.
    $user_platform = detect_user_platform();
    
    $dl_link = platform_download_link();
    $dl_plat = user_download_platform();
    $button_label = omit_zeroes(SiteData::get()->data()['latest_stable_version'][$dl_plat]);
    if (!$button_label) {
        $button_label = 'Download';
    }
    $download_link ="<a href='$dl_link' class='quick-download-link' "
            ."title='Download latest stable release'>$button_label <span "
            ."class='downarrow'>&#x21E3;</span></a>";
?>
<div id="dengine-topbar">
    <ul class="site-navigation">
        <li><a href="<?php echo(SITE_ROOT); ?>" class="site-link">Doomsday<span class="aux-word"> Engine</span></a></li>
        <li class="quick-download"><?php echo($download_link); ?></li>
        <li class="supplementary"><a href="<?php echo(SITE_ROOT); ?>/builds">Builds</a></li>
        <li class="supplementary"><a href="<?php echo(SITE_ROOT."/addons"); ?>">Add-ons</a></li>
        <li><a href="https://manual.dengine.net">Manual</a></li>
        <li>&middot;</li>
        <li><a href="https://talk.dengine.net">Forums</a></li>
        <li class="supplementary"><a href="<?php echo(SITE_ROOT); ?>/support">Support</a></li>
        <li><a href="https://tracker.dengine.net/projects/deng"><span class="aux-word">Bug </span>Tracker</a></li>
        <li><a href="<?php echo(SITE_ROOT).'/recent_posts'; ?>" class="blog-link">Blog</a></li>
    </ul>
</div>
