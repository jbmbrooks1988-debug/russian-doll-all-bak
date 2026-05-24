<?php
header('Content-Type: application/json');

$news_file = '../data/news.txt';
$headlines = [];

if (file_exists($news_file)) {
    $lines = file($news_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    $in_headlines_section = false;
    foreach ($lines as $line) {
        if (strpos($line, "FINANCIAL NEWS HEADLINES") !== false) {
            $in_headlines_section = true;
            continue;
        }
        if ($in_headlines_section && strpos($line, "---") === 0) { // End of header or start/end of section
            continue;
        }
        if ($in_headlines_section && !empty(trim($line))) {
            $headlines[] = trim($line);
        }
    }
    // Remove header and footer lines that might have been included.
    // This is a bit of a hack without proper parsing, but works for the current format.
    if (count($headlines) > 0 && strpos($headlines[0], "Market Update:") !== false) {
        array_shift($headlines);
    }
    while (count($headlines) > 0 && strpos(end($headlines), "--------") !== false) {
        array_pop($headlines);
    }

} else {
    $headlines[] = 'News file not found.';
}

echo json_encode(['headlines' => $headlines]);
?>
