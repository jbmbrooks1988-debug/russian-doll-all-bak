<?php
include_once __DIR__ . '/helpers.php'; // Contains parse_corp_for_analysis
include_once __DIR__ . '/gamestate_utils.php'; // Contains read_gamestate, write_gamestate, read_wsr_clock, write_wsr_clock, read_settings
include_once __DIR__ . '/debug_log.php'; // Include debug logging utility

$response = ['status' => 'success', 'message' => 'Tick processed.'];
$day_passed = false;

try {
    $gamestate = read_gamestate();
    $wsr_clock = read_wsr_clock();
    $settings = read_settings();

    // The last_tick_time needs to be from gamestate.txt
    $last_tick_time = $gamestate['last_tick_time'] ?? time();
    $current_time = time();
    $real_seconds_elapsed = $current_time - $last_tick_time;
    
    // Check if called from setup_corporations (where last_tick_time is temporarily 0)
    $is_initial_setup_call = ($last_tick_time === 0);

    if ($real_seconds_elapsed <= 0 && !$is_initial_setup_call) {
        // If no real time has elapsed and it's not an initial setup call, do nothing.
        write_debug_log("php/game_tick.php: No real time elapsed or initial setup call, returning early. real_seconds_elapsed: $real_seconds_elapsed, is_initial_setup_call: " . ($is_initial_setup_call ? 'true' : 'false'));
        return $response; // Return response for parent script
    }

    $ticker_speed = $settings['ticker_speed']; // Get ticker speed from settings.txt
    $seconds_per_second = [
        'sec' => 3600,
        'min' => 60,
        'hour' => 1,
        'day' => 86400,
        'cent' => 36000 // Added for completeness, though not typically used as 'per second'
    ];
    // Default rate if ticker_speed is unknown or invalid
    $rate = $seconds_per_second[$ticker_speed] ?? $seconds_per_second['sec']; 

    // For initial setup call, ensure at least one day passes for analysis
    $game_seconds_to_add = $is_initial_setup_call ? $seconds_per_second['day'] : $rate * $real_seconds_elapsed;
    
    $date = new DateTime();
    $date->setDate($wsr_clock['year'], $wsr_clock['month'], $wsr_clock['day']);
    $date->setTime($wsr_clock['hour'], $wsr_clock['minute'], $wsr_clock['second']);
    
    $old_day = $date->format('d');
    $date->modify('+' . round($game_seconds_to_add) . ' seconds');
    write_debug_log("php/game_tick.php: DateTime object after modification: " . $date->format('Y-m-d H:i:s'));
    $new_day = $date->format('d');
    
    // If it's an initial setup, force day_passed to true to run economic events
    if($old_day != $new_day || $is_initial_setup_call) {
        $day_passed = true;
    }

    // Update the wsr_clock array
    $wsr_clock['year'] = (int)$date->format('Y');
    $wsr_clock['month'] = (int)$date->format('m');
    $wsr_clock['day'] = (int)$date->format('d');
    $wsr_clock['hour'] = (int)$date->format('H');
    $wsr_clock['minute'] = (int)$date->format('i');
    $wsr_clock['second'] = (int)$date->format('s');
    // Centisecond logic will need to be re-evaluated if needed, for now keep simple.
    
    write_debug_log("php/game_tick.php: WSR Clock array before writing: " . print_r($wsr_clock, true));
    // Save updated wsr_clock.txt
    write_wsr_clock($wsr_clock);

    // Update last_tick_time in gamestate.txt
    $gamestate['last_tick_time'] = $current_time;
    write_debug_log("php/game_tick.php: Gamestate array before writing: " . print_r($gamestate, true));
    write_gamestate($gamestate);

    // --- If a day has passed (or forced by initial setup), run economic events ---
    if ($day_passed) {
        write_debug_log("php/game_tick.php: Day passed or initial setup, running economic events.");
        $corp_base_path = '../data/corporations/generated/';
        $news_headlines = [];

        if (is_dir($corp_base_path)) {
            $corporation_dirs = scandir($corp_base_path);
            foreach ($corporation_dirs as $corp_ticker) {
                if ($corp_ticker === '.' || $corp_ticker === '..' || $corp_ticker === 'martian_corporations.txt') continue; 
                
                $corp_dir_path = $corp_base_path . $corp_ticker . '/';
                if (!is_dir($corp_dir_path)) continue;

                $corp_file = $corp_dir_path . $corp_ticker . '.txt';
                $weights_file = $corp_dir_path . 'weights.txt';
                $history_file = $corp_dir_path . 'price_history.txt';

                $corp_data = parse_corp_for_analysis($corp_file, $corp_ticker);
                if (!$corp_data) continue;

                $risk = 50;
                if (file_exists($weights_file) && preg_match('/risk\s*(\d+)/', file_get_contents($weights_file), $matches)) {
                    $risk = (int)$matches[1];
                }
                
                $book_value = $corp_data['total_assets'] - $corp_data['total_liabilities'];
                $book_value_per_share = ($corp_data['shares_outstanding'] > 0) ? $book_value / $corp_data['shares_outstanding'] : 0;
                $market_cap = $corp_data['current_stock_price'] * $corp_data['shares_outstanding'];
                $market_cap_multiplier = ($market_cap > 0) ? 1.0 + (log10($market_cap) - 3) * 0.05 : 1.0;
                $leverage_factor = ($corp_data['debt_to_equity'] > 1.0) ? 1.0 - ($corp_data['debt_to_equity'] - 1.0) * 0.1 : 1.0 + (0.5 - $corp_data['debt_to_equity']) * 0.05;
                $bias_factor = 0.5 + ($risk / 100.0) * 1.0;
                $fundamental_value = $book_value_per_share * $market_cap_multiplier * $leverage_factor * $bias_factor;
                $new_price = ($fundamental_value * 0.7) + ($corp_data['current_stock_price'] * 0.3);
                if ($new_price < 0.1) $new_price = 0.1;

                file_put_contents($corp_file, preg_replace('/(Current Stock Price:\s+)[0-9\.]+/', "$1" . sprintf("%.2f", $new_price), file_get_contents($corp_file)));

                $old_price = $corp_data['current_stock_price'];
                $change_percent = ($old_price > 0) ? (($new_price - $old_price) / $old_price) * 100.0 : 0.0;
                file_put_contents($history_file, $date->format('Y-m-d') . "," . sprintf("%.2f,%.2f,%.2f%%", $old_price, $new_price, $change_percent) . "\n", FILE_APPEND);
                
                $news_headlines[] = ['name' => $corp_data['name'], 'ticker' => $corp_ticker, 'price' => $new_price, 'change' => $change_percent];
            }
        }
        
        usort($news_headlines, fn($a, $b) => abs($b['change']) <=> abs($a['change']));
        $news_content = "FINANCIAL NEWS HEADLINES\n========================\nMarket Update: " . $date->format('Y-m-d H:i:s') . "\n------------------------\n";
        foreach(array_slice($news_headlines, 0, 10) as $news) {
            $news_content .= sprintf("%s (%s): $%.2f (%s%.2f%%)\n", $news['name'], $news['ticker'], $news['price'], $news['change'] >= 0 ? '+' : '', $news['change']);
        }
        file_put_contents('../data/news.txt', $news_content);
        $response['message'] = ($is_initial_setup_call ? 'Initial analysis complete. ' : '') . 'New day processed. Economic events run.';
    }

    $response['gamestate'] = array_merge($gamestate, $wsr_clock, $settings); // Combine for frontend response
    write_debug_log("php/game_tick.php: Final combined response before JSON encoding: " . print_r($response['gamestate'], true));

} catch (Exception $e) {
    // http_response_code(500); // Only set header if called directly
    write_debug_log("php/game_tick.php: Error: " . $e->getMessage());
    $response = ['status' => 'error', 'message' => $e->getMessage()];
}

// Check if the script is being included or called directly
if (basename(__FILE__) == basename($_SERVER['PHP_SELF'])) {
    // Called directly (e.g., via fetch from index.html)
    header('Content-Type: application/json');
    echo json_encode($response);
} else {
    // Included by another script (e.g., setup_corporations.php)
    return $response;
}

?>
