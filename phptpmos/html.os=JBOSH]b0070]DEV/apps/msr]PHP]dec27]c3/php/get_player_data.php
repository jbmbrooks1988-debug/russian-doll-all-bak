<?php
header('Content-Type: application/json');
include_once __DIR__ . '/gamestate_utils.php';

$player_name = isset($_GET['player']) ? $_GET['player'] : null;
error_log("php/get_player_data.php: Fetching data for player: " . $player_name);

if (!$player_name) {
    echo json_encode(['error' => 'Player name not provided.']);
    exit;
}

$player_dir = '../data/players/' . $player_name;
$balance_sheet_path = $player_dir . '/balance_sheet.txt';
$portfolio_path = $player_dir . '/portfolio.txt';

$cash = 0.0;
$portfolio_value = 0.0;
$debt = 0.0; // Assuming debt is always negative or 0 in balance sheet display
$ticker_on = false; // Default value

// Load gamestate to get ticker_on status from gamestate.txt
$gamestate = read_gamestate();
$player_ticker_key = $player_name . '_ticker_on';
$ticker_on = $gamestate[$player_ticker_key] ?? false;

// Read Balance Sheet
if (file_exists($balance_sheet_path)) {
    $lines = file($balance_sheet_path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        if (strpos($line, "Cash (CD's)..") !== false) {
            preg_match('/Cash \(CD\'s\)\.\.\s*([\-]?\d+\.?\d*)/', $line, $matches);
            if (isset($matches[1])) $cash = (float)$matches[1];
        }
        // Assuming "Less Debt" is parsed as a positive value, then subtracted later
        if (strpos($line, "Less Debt....") !== false) {
            preg_match('/Less Debt\.\.\.\.\s*([\-]?\d+\.?\d*)/', $line, $matches);
            if (isset($matches[1])) $debt = abs((float)$matches[1]); // Ensure positive for calculation
        }
    }
}

// Read Portfolio
if (file_exists($portfolio_path)) {
    $lines = file($portfolio_path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        if (strpos($line, "TOTAL STOCK PORTFOLIO VALUE:") !== false) {
            preg_match('/TOTAL STOCK PORTFOLIO VALUE:\s*([0-9,\.\-]+)/', $line, $matches);
            if (isset($matches[1])) $portfolio_value = (float)str_replace(',', '', $matches[1]);
        }
    }
}

echo json_encode([
    'player_name' => $player_name,
    'cash' => $cash,
    'portfolio_value' => $portfolio_value,
    'debt' => $debt,
    'total_assets' => $cash + $portfolio_value,
    'net_worth' => ($cash + $portfolio_value) - $debt,
    'ticker_on' => $ticker_on
]);

?>