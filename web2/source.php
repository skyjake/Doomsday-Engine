<?php
require_once('include/template.inc.php');
$page_title = 'Source Code';
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
    <h1>Source Packages</h1>
    <?php 
    generate_badges('source', BT_STABLE);
    generate_badges('source', BT_UNSTABLE);
    ?>
</article>
</div>
<div class="block">
<article>            
    <h1>New to Doomsday Source?</h1>
    <p>The best place to begin is the <a href="https://manual.dengine.net/devel">Development section of the Manual</a>. Here are a couple of important pages to read first:</p>
    <ul>
        <li><a href="https://manual.dengine.net/devel/getting_started">Getting started</a></li>
        <li><a href="https://manual.dengine.net/devel/guidelines">Developer guidelines</a></li>
    </ul>
    <h2>API Documentation</h2>
    <ul>
        <li><a href="http://source.dengine.net/apidoc/sdk">Doomsday 2 SDK</a></li>
        <li><a href="http://source.dengine.net/apidoc/api">Runtime API</a></li>
    </ul>
    <h2>Git Repositories</h2>
    <ul>
        <li><a href="https://github.com/skyjake/Doomsday-Engine.git">GitHub: skyjake/Doomsday-Engine</a></li>
        <li><a href="http://source.dengine.net/codex">Commit tag index</a></li>
    </ul>                
</article>
</div>
        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
