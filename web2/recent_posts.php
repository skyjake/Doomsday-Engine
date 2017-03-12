<?php
require_once('include/template.inc.php');
$page_title = 'Recent Blog Posts';
generate_page_header($page_title);

function cmp_post($a, $b)
{
    return strcmp($b->date, $a->date);
}

function generate_post_blocks()
{
    // Fetch the cached blog entries.
    cache_try_load(cache_key('news', 'news'), -1);
    $news = json_decode(cache_get());            
    cache_try_load(cache_key('news', 'dev'), -1);
    $dev = json_decode(cache_get());

    $posts = array_merge($news->posts, $dev->posts);
    usort($posts, "cmp_post");
    
    for ($i = 0; $i < count($posts); $i++) {
        $post = $posts[$i];
        $url = $post->url;
        $category = $post->categories[0];
        $cat_link = '/blog/category/'.$category->slug;
        $date = reformat_date($post->date);
        $tags = '';
        foreach ($post->tags as $tag) {
            $tags .= " <div class='post-tag'><a href='/blog/tag/$tag->slug'>$tag->title</a></div>";    
        }
        echo(
        "<div class='block blog-post'>
            <article>
                <h1><a href='$url'>$post->title</a></h1>
                <div class='post-metadata'>                    
                    <a class='post-category' href='$cat_link'>$category->title</a> &middot; 
                    <time datetime='$post->date'>$date</time> &middot;
                    <span class='post-author'>".$post->author->name."</span> &middot; 
                    $tags                    
                </div>
                <div class='post-content'>$post->content</div>
            </article>
        </div>\n");
    }
}

?>
<body>
    <?php 
    include('include/topbar.inc.php'); 
    generate_page_title($page_title);
    ?>
    <div id='content'>
        <div id='page-content'>
            <?php generate_post_blocks(); ?>      
            <div class='block'>
                <article>
                    <p><a href="/blog">More blog posts &rarr;</a></p>
                </article>
            </div>                  
        </div>
        <?php generate_sidebar(); ?>
    </div>
    <?php generate_sitemap(); ?>
</body>
