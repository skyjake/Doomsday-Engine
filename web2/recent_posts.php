<?php
require_once('include/template.inc.php');
setlocale(LC_ALL, 'en_US.UTF-8');
$page_title = 'Recent Blog Posts';

function cmp_post($a, $b)
{
    return strcmp($b->date, $a->date);
}

function fetch_posts()
{
    // Fetch the cached blog entries.
    cache_try_load(cache_key('news', 'news'), -1);
    $news = json_decode(cache_get());            
    cache_try_load(cache_key('news', 'dev'), -1);
    $dev = json_decode(cache_get());

    $posts = array_merge($news->posts, $dev->posts);
    usort($posts, "cmp_post");
    return $posts;
}

function generate_post_blocks()
{
    $posts = fetch_posts();
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
                <h1 id='$post->slug'><a href='$url'>$post->title</a></h1>
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

function generate_blog_rss_feed()
{
    header('Content-Type: application/rss+xml');
    
    $posts = fetch_posts();
    $latest_ts = time();
    if (count($posts) > 0) {        
        $latest_ts = timestamp_from_date($posts[0]->date);
    }
    $rfc_latest = gmstrftime(RFC_TIME, $latest_ts);
    echo("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        ."<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
        .'<channel>'
        .'<title>Doomsday Blog</title>'
        .'<link>http://dengine.net/recent_posts</link>'
        .'<atom:link href="http://dengine.net/recent_posts?format=rss" rel="self" type="application/rss+xml" />'
        .'<description>Recent posts on the Doomsday Engine development blog</description>'
        .'<language>en-us</language>'
        ."<webMaster>webmaster@dengine.net (Jaakko Keränen)</webMaster>"
        ."<lastBuildDate>$rfc_latest</lastBuildDate>"
        .'<generator>http://dengine.net/recent_posts</generator>'
        .'<ttl>180</ttl>'); # 3 hours    
    for ($i = 0; $i < count($posts); $i++) {
        $post = $posts[$i];
        $rfc_time = gmstrftime(RFC_TIME, timestamp_from_date($post->date));
        $content = $post->content;
        $author = $post->author->name;
        echo("\n<item>\n"
            ."<title>$post->title_plain</title>\n"
            ."<link>http://dengine.net/recent_posts#$post->slug</link>\n"
            ."<author>$author@dengine.net ($author)</author>\n"
            ."<pubDate>$rfc_time</pubDate>\n"
            #."<atom:summary><![CDATA[$summary]]></atom:summary>\n"
            ."<description><![CDATA[$content]]></description>\n"
            ."<guid isPermaLink=\"false\">$post->slug</guid>\n"
            ."</item>\n");
    }
    echo('</channel></rss>');
}

function generate_html()
{
    generate_page_header($page_title);
    echo("<body>");
    include('include/topbar.inc.php'); 
    generate_page_title($page_title);
    echo("<div id='content'><div id='page-content'>");    
    generate_post_blocks();
    echo("<div class='block'>
                    <article>
                        <p><a href='/blog'>More blog posts &rarr;</a></p>
                    </article>
                </div>                  
            </div>");
    generate_sidebar();
    echo("</div>");
    generate_sitemap();
    echo("</body>");
}

if ($_SERVER['REQUEST_METHOD'] == 'GET') {
    if ($format = $_GET['format']) {    
        if ($format == 'rss') {
            generate_blog_rss_feed();
            exit;
        }
    }
}
// Otherwise...
generate_html();
