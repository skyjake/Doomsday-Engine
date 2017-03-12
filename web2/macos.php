<?php
require_once('include/template.inc.php');
$page_title = 'macOS Downloads';
generate_page_header($page_title);
?>
<body>
    <?php 
    include('include/topbar.inc.php'); 
    generate_page_title($page_title);
    ?>
    <div id='content'>
        <div id='page-content'>
            <div class="block">
                <article id="overview">
                    <h1><?php echo($page_title); ?></h1>
                    <p>Doomsday Engine can run on recent 64-bit versions of macOS. If your macOS is more than two years old, please check out the <a href="/manual/supported_platforms">older Doomsday releases</a>.</p>
                </article>
            </div>
            <div class="block">
                <article>
                    <h1>Stable</h1>
                    <?php generate_badges('mac10_8-x86_64', BT_STABLE); ?>
                </article>
            </div>
            <div class="block">
                <article>
                    <h1>Release Candidate</h1>
                    <p>Release candidate of the next upcoming stable build.</p>
                    <?php generate_badges('mac10_10-x86_64', BT_CANDIDATE); ?>
                </article>
            </div>
            <div class="block">
                <article>
                    <h1>Unstable / Nightly</h1>
                    <p>Unstable builds are made automatically every day when changes are committed to the <a href="source">source repository</a>. They contain work-in-progress code and sometimes may crash on you.</p>
                    <?php generate_badges('mac10_10-x86_64', BT_UNSTABLE); ?>
                </article>
            </div>
        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
