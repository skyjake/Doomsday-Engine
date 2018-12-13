<?php
require_once('include/template.inc.php');
$page_title = 'Windows Downloads';
generate_page_header($page_title);
?>
<body>
    <?php
    include(DENG_LIB_DIR.'/topbar.inc.php');
    generate_page_title($page_title);
    ?>
    <div id='content'>
        <div id='page-content'>
            <div class="block">
                <article id="overview">
                    <h1><?php echo($page_title); ?></h1>
                    <p>Doomsday Engine can run on all modern Microsoft Windows &reg; desktop operating systems (Vista or later). There are separate installation packages available for 32-bit and 64-bit versions of Windows. Requires a GPU that supports OpenGL 3.3.</p>
                </article>
            </div>
            <div class="block">
                <article>
                    <h1>Stable</h1>
                    <p>This is the latest stable version. <?php echo(release_notes_link('win-x64')); ?></p>
                    <?php
                    generate_badges('win-x64', BT_STABLE);
                    generate_badges('win-x86', BT_STABLE);
                    ?>
                </article>
            </div>
            <div class="block"
                <?php if (!is_release_candidate_available('win-x64')) echo("style='display:none';"); ?> >
                <article>
                    <h1>Release Candidate</h1>
                    <p>Release candidate of the next upcoming stable build.</p>
                    <?php
                    generate_badges('win-x64', BT_CANDIDATE);
                    generate_badges('win-x86', BT_CANDIDATE);
                    ?>
                </article>
            </div>
            <div class="block">
                <article>
                    <h1>Unstable / Nightly</h1>
                        <p>Unstable builds are made automatically every day when changes are committed to the <a href="source">source repository</a>. They contain work-in-progress code and sometimes may crash on you. Change logs can be found in the <a href="/builds">Autobuilder</a>.</p>
                    <?php
                    generate_badges('win-x64', BT_UNSTABLE);
                    generate_badges('win-x86', BT_UNSTABLE);
                    ?>
                </article>
            </div>
        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
