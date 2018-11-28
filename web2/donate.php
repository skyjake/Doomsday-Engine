<?php
require_once('include/template.inc.php');
$page_title = '';
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
                <article>
                    <h1>Donate to support the project</h1>
                    <p>Doomsday Engine is a long-running open source project.
                        Since its inception in 1999, it has been fueled by nostalgia
                        and the passion of its developers. However, those things 
                        unfortunately cannot pay for web hosting, build server upgrades,
                        code signing keys, and other running costs.</p>
                    <p>If you enjoy Doomsday and wish to see its development continue
                        at full speed, please consider making a donation.
                    </p>
                    <div class='undersigned'>&mdash; Jaakko Ker&auml;nen
                        <div class='role'>Project founder</div>
                    </div>
                </article>
            </div>
            <div class="block">
                <article>
                    <h1>PayPal</h1>
                    <p><a href="http://sourceforge.net/p/deng/donate/?source=navbar">Support Doomsday Engine development &rarr;</a></p>
                </article>
            </div>
        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
