<?php
require_once('include/template.inc.php');
$page_title = 'Add-ons';
generate_page_header($page_title);
?>
<body>
    <?php 
    include('include/topbar.inc.php'); 
    generate_page_title($page_title);
    ?>
    <div id='content'>
        <div id='page-content'>

        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
