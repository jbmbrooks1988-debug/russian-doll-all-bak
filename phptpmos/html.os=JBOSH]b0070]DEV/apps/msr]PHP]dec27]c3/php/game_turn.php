<?php
header('Content-Type: application/json');
include_once __DIR__ . '/gamestate_utils.php'; // Contains read_gamestate and write_gamestate

$response = ['status' => 'error', 'message' => 'An unknown error occurred.'];

try {
    $gamestate = read_gamestate();

    if (!isset($gamestate['players']) || count($gamestate['players']) == 0) {
        throw new Exception("No players found in gamestate.");
    }

    // Advance the player turn
    $num_players = count($gamestate['players']);
    $gamestate['current_player_index'] = ($gamestate['current_player_index'] + 1) % $num_players;
    
    // Save the updated gamestate
    write_gamestate($gamestate);

    $response = [
        'status' => 'success',
        'message' => 'Player turn advanced.',
        'new_player_index' => $gamestate['current_player_index'],
        'next_player_name' => $gamestate['players'][$gamestate['current_player_index']]
    ];

} catch (Exception $e) {
    http_response_code(500);
    $response = ['status' => 'error', 'message' => $e->getMessage()];
}

echo json_encode($response);
?>