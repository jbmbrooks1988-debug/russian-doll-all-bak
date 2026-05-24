<?php
header('Content-Type: application/json');

$gamestate_file = '../data/gamestate.json';

if (file_exists($gamestate_file)) {
    $gamestate_json = file_get_contents($gamestate_file);
    $gamestate = json_decode($gamestate_json, true);

    // Get the ticker speed
    $ticker_speed = $gamestate['ticker_speed'];

    // Define how many game seconds pass per real-world second for each speed
    $seconds_per_second = [
        'sec' => 3600,      // 1 game hour per real second
        'min' => 60,        // 1 game minute per real second
        'hour' => 1,        // 1 game second per real second
        'day' => 1.0 / 24.0, // 1 game day per real second
    ];

    $rate = $seconds_per_second[$ticker_speed] ?? 0;

    // This script is called every few seconds, let's assume a 5 second interval
    $real_seconds_elapsed = 5; 
    
    $game_seconds_to_add = $rate * $real_seconds_elapsed;

    // Create a DateTime object to handle the time addition
    $date = new DateTime();
    $date->setDate($gamestate['year'], $gamestate['month'], $gamestate['day']);
    $date->setTime($gamestate['hour'], $gamestate['minute'], $gamestate['second']);
    
    $date->modify('+' . round($game_seconds_to_add) . ' seconds');

    // Update the gamestate with the new time
    $gamestate['year'] = (int)$date->format('Y');
    $gamestate['month'] = (int)$date->format('m');
    $gamestate['day'] = (int)$date->format('d');
    $gamestate['hour'] = (int)$date->format('H');
    $gamestate['minute'] = (int)$date->format('i');
    $gamestate['second'] = (int)$date->format('s');

    // Save the updated gamestate
    file_put_contents($gamestate_file, json_encode($gamestate, JSON_PRETTY_PRINT));

    echo json_encode(['success' => true, 'new_time' => $gamestate]);

} else {
    echo json_encode(['error' => 'Gamestate file not found.']);
}
?>
