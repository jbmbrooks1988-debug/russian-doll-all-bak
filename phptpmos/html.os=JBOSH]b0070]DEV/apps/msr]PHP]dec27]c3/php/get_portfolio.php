<?php
header('Content-Type: application/json');

$player_name = isset($_GET['player']) ? $_GET['player'] : 'Player1';

// Sanitize player name
$sanitized_player_name = preg_replace('/[^a-zA-Z0-9_-]/', '', $player_name);
if (empty($sanitized_player_name)) {
    http_response_code(400);
    echo json_encode(['error' => 'Invalid player name provided.']);
    exit;
}

$portfolio_path = '../data/players/' . $sanitized_player_name . '/portfolio.txt';

$portfolio_data = [
    'stocks' => [],
    'bonds' => [],
    'total_stock_value' => 0,
    'error' => null
];

if (!file_exists($portfolio_path)) {
    $portfolio_data['error'] = 'Portfolio file not found for player ' . $sanitized_player_name . '. Please run setup.';
    echo json_encode($portfolio_data);
    exit;
}

$lines = file($portfolio_path, FILE_IGNORE_NEW_LINES);
$parsing_section = 'none'; // none, stocks, bonds

foreach($lines as $line) {
    if (trim($line) === '') continue;

    if (strpos($line, 'STOCK PORTFOLIO FOR') !== false) {
        $parsing_section = 'stocks';
        continue;
    }
    if (strpos($line, 'BOND PORTFOLIO FOR') !== false) {
        $parsing_section = 'bonds';
        continue;
    }
    if (strpos($line, 'TOTAL STOCK PORTFOLIO VALUE:') !== false) {
        sscanf($line, "TOTAL STOCK PORTFOLIO VALUE: %d", $portfolio_data['total_stock_value']);
        continue;
    }

    if ($parsing_section === 'stocks') {
        // Example line: "FULL METAL BITS FMB 100% 19.53 0.0% 1953 NOT TRADED"
        // For simplicity, we'll just extract the raw line for now.
        // A more robust parser would be needed for a detailed breakdown.
        if (strpos($line, '----') === false && strpos($line, 'COMPANY SYM') === false) {
            $portfolio_data['stocks'][] = $line;
        }
    } elseif ($parsing_section === 'bonds') {
        if (strpos($line, '----') === false && strpos($line, 'BOND ISSUER') === false && strpos($line, 'OWNS NO BONDS') === false) {
             $portfolio_data['bonds'][] = $line;
        }
    }
}

echo json_encode($portfolio_data);
?>