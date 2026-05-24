<?php
header('Content-Type: application/json');
include_once __DIR__ . '/gamestate_utils.php';

$player_name = isset($_POST['playerName']) ? $_POST['playerName'] : null;
$starting_cash = isset($_POST['startingCash']) ? (float)$_POST['startingCash'] : 0.0;
$ticker_speed = isset($_POST['tickerSpeed']) ? $_POST['tickerSpeed'] : 'sec'; // Default to 'sec'

if (!$player_name || empty(trim($player_name))) {
    echo json_encode(['status' => 'error', 'message' => 'Player name is required.']);
    exit;
}

if ($starting_cash <= 0) {
    echo json_encode(['status' => 'error', 'message' => 'Starting cash must be a positive number.']);
    exit;
}

// Sanitize player name to prevent directory traversal issues
$sanitized_player_name = preg_replace('/[^a-zA-Z0-9_-]/', '', $player_name);
if (empty($sanitized_player_name)) {
    echo json_encode(['status' => 'error', 'message' => 'Invalid player name. Use only alphanumeric characters, underscores, and hyphens.']);
    exit;
}

$player_dir = '../data/players/' . $sanitized_player_name;
$balance_sheet_path = $player_dir . '/balance_sheet.txt';
$portfolio_path = $player_dir . '/portfolio.txt';

try {
    // --- Initialize/Update gamestate.txt ---
    $gamestate = read_gamestate();
    $gamestate['players'] = [$sanitized_player_name]; // Overwrite with current player for new game setup
    $gamestate['current_player_index'] = 0; // First player is always index 0
    // Player-specific ticker_on status
    $gamestate[$sanitized_player_name . '_ticker_on'] = false;
    $gamestate['last_tick_time'] = time(); // Set initial last tick time
    write_gamestate($gamestate);



    // --- Initialize/Update setting.txt ---
    $settings = read_settings(); // Gets defaults if file doesn't exist
    $settings['ticker_speed'] = $ticker_speed;
    write_settings($settings);


    if (!is_dir('../data/players')) {
        mkdir('../data/players', 0777, true);
    }
    
    if (!is_dir($player_dir)) {
        mkdir($player_dir, 0777, true);
    } else {
        // Player already exists, which might be okay or might be an error depending on game logic
        // For now, we'll just overwrite their files.
    }

    // --- Create balance_sheet.txt ---
    $balance_sheet_content = "My Balance Sheet:\n";
    $balance_sheet_content .= "Cash (CD's)..\t\t" . sprintf("%.2f", $starting_cash) . "\n";
    $balance_sheet_content .= "Other Assets....\t\t0.00\n";
    $balance_sheet_content .= "Total Assets.....\t\t" . sprintf("%.2f", $starting_cash) . "\n";
    $balance_sheet_content .= "Less Debt....\t\t-0.00\n";
    $balance_sheet_content .= "Net Worth........\t\t" . sprintf("%.2f", $starting_cash) . "\n";
    
    if (file_put_contents($balance_sheet_path, $balance_sheet_content) === false) {
        throw new Exception("Failed to write balance sheet for player " . $sanitized_player_name);
    }

    // --- Create portfolio.txt ---
    $upper_player_name = strtoupper($sanitized_player_name);
    $portfolio_content = "STOCK & BOND PORTFOLIO LISTING FOR " . $upper_player_name . "\n";
    $portfolio_content .= "STOCK PORTFOLIO FOR " . $upper_player_name . " (In U.S. Dollars)\n";
    $portfolio_content .= "STOCK           SHARES      VALUE\n";
    $portfolio_content .= "TOTAL STOCK PORTFOLIO VALUE: 0\n";
    $portfolio_content .= "BOND PORTFOLIO FOR " . $upper_player_name . " (In U.S. Dollars)\n";
    $portfolio_content .= "BOND            VALUE\n";
    $portfolio_content .= $sanitized_player_name . " OWNS NO BONDS ....
";
    
    if (file_put_contents($portfolio_path, $portfolio_content) === false) {
        throw new Exception("Failed to write portfolio for player " . $sanitized_player_name);
    }

    echo json_encode(['status' => 'success', 'message' => 'Player ' . $sanitized_player_name . ' created successfully.']);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}
?>
