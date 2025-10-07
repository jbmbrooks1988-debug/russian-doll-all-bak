<?php
// Reading posts from the CSV index file
$posts = [];
$index_file = 'posts/index.csv';

if (file_exists($index_file)) {
    $file = fopen($index_file, 'r');
    while (($line = fgetcsv($file)) !== false) {
        $posts[] = [
            'timestamp' => $line[0],
            'filename' => $line[1],
            'image_filename' => isset($line[2]) ? $line[2] : ''
        ];
    }
    fclose($file);
}

// Reverse posts to show newest first
$posts = array_reverse($posts);
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Forum Posts</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
        }
        h1 {
            text-align: center;
        }
        .post {
            margin-bottom: 15px;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
        }
        .post-meta {
            color: #555;
            font-size: 0.9em;
        }
        .post-image {
            max-width: 100%;
            height: auto;
            margin-top: 10px;
        }
    </style>
</head>
<body>
    <h1>Forum Posts</h1>
    <p><a href="/">Back to Forum</a></p>
    <div class="post-list">
        <?php if (empty($posts)): ?>
            <p>No posts available.</p>
        <?php else: ?>
            <?php foreach ($posts as $post): ?>
                <div class="post">
                    <div class="post-meta">
                        Posted on: <?php echo date('Y-m-d H:i:s', $post['timestamp']); ?>
                    </div>
                    <?php
                    $content_file = 'posts/' . $post['filename'];
                    if (file_exists($content_file)) {
                        $content = file_get_contents($content_file);
                        echo '<p>' . htmlspecialchars($content) . '</p>';
                    } else {
                        echo '<p>Content not found.</p>';
                    }
                    
                    // Display image if available
                    if (!empty($post['image_filename'])) {
                        $image_path = 'uploads/' . $post['image_filename'];
                        if (file_exists($image_path)) {
                            echo '<img src="' . htmlspecialchars($image_path) . '" alt="Post image" class="post-image">';
                        } else {
                            echo '<p>Image not found.</p>';
                        }
                    }
                    ?>
                </div>
            <?php endforeach; ?>
        <?php endif; ?>
    </div>
</body>
</html>
