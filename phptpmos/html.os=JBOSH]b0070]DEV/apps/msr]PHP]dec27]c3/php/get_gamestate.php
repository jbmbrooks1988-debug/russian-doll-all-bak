<?php
header('Content-Type: application/json');
include_once __DIR__ . '/gamestate_utils.php';
include_once __DIR__ . '/debug_log.php'; // Include debug logging utility

try {
    // Read individual components
    $gamestate_data = read_gamestate();
    $wsr_clock_data = read_wsr_clock();
    $settings_data = read_settings();

    // Calculate real-time adjusted game time
    $last_tick_time = $gamestate_data['last_tick_time'] ?? time();
    $current_time = time();
    $real_seconds_elapsed = $current_time - $last_tick_time;
    
    // Get the base time from the clock file
    $base_time = new DateTime();
    $base_time->setDate($wsr_clock_data['year'], $wsr_clock_data['month'], $wsr_clock_data['day']);
    $base_time->setTime($wsr_clock_data['hour'], $wsr_clock_data['minute'], $wsr_clock_data['second']);

    // Get the ticker speed setting
    $ticker_speed = $settings_data['ticker_speed'] ?? 'sec';
    $seconds_per_second = [
        'sec' => 3600, // 1 real sec = 1 hour of game time
        'min' => 60,   // 1 real sec = 1 min of game time  
        'hour' => 1,   // 1 real sec = 1 sec of game time
        'day' => 86400, // 1 real sec = 24 hours of game time
    ];
    // Default rate if ticker_speed is unknown or invalid
    $rate = $seconds_per_second[$ticker_speed] ?? $seconds_per_second['sec'];

    // Advance game time based on real time passed and ticker speed (assuming ticker is active)
    // We'll assume the ticker is running by default as per your requirements
    $game_seconds_to_add = $rate * $real_seconds_elapsed;
    $base_time->modify('+' . round($game_seconds_to_add) . ' seconds');
    
    // Combine into a single response array
    $response = [
        'current_player_index' => $gamestate_data['current_player_index'],
        'players' => $gamestate_data['players'],
        'last_tick_time' => $gamestate_data['last_tick_time'],
        'ticker_speed' => $settings_data['ticker_speed'],
        // Include real-time adjusted clock data
        'year' => (int)$base_time->format('Y'),
        'month' => (int)$base_time->format('m'),
        'day' => (int)$base_time->format('d'),
        'hour' => (int)$base_time->format('H'),
        'minute' => (int)$base_time->format('i'),
        'second' => (int)$base_time->format('s'),
    ];

    write_debug_log("php/get_gamestate.php: Response array before JSON encoding: " . print_r($response, true));

    echo json_encode($response);

} catch (Exception $e) {
    http_response_code(500);
    write_debug_log("php/get_gamestate.php: Error: " . $e->getMessage());
    echo json_encode(['error' => $e->getMessage()]);
}

?>
