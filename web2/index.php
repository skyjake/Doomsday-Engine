<?php
require_once('include/template.inc.php');
generate_page_header('Doomsday Engine');
?>
<body>
    <?php include('include/topbar.inc.php'); ?>
    <div id='content'>
        <div id='welcome'>
            <div class='main-logo'></div>
            <div class='intro'>
                <section>
                    <h1>WELCOME BACK TO THE 1990s</h1>
                    <p>Doomsday Engine is a Doom / Heretic / Hexen port with enhanced graphics</p>
                    <div class="download-button">
                        <?php generate_download_button(); ?>
                    </div>
                </section>
            </div>
        </div>
    </div>
</body>
