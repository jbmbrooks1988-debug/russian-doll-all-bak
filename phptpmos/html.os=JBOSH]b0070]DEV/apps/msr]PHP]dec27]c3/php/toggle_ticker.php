<?php
header('Content-Type: application/json');
include_once __DIR__ . '/gamestate_utils.php';

$response = ['status' => 'error', 'message' => 'An unknown error occurred.'];

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);
    $player_name = $input['player'] ?? null;

    if ($player_name === null) {
        http_response_code(400);
        $response['message'] = 'Player name not provided.';
        echo json_encode($response);
        exit();
    }

    try {
        $gamestate = read_gamestate();

        $player_ticker_key = $player_name . '_ticker_on';
        $new_ticker_status = !($gamestate[$player_ticker_key] ?? false);
        $gamestate[$player_ticker_key] = $new_ticker_status;
        
        write_gamestate($gamestate);

        $response = [
            'status' => 'success',
            'message' => 'Ticker status toggled successfully.',
            'new_ticker_status' => $new_ticker_status
        ];

    } catch (Exception $e) {
        http_response_code(500);
        $response['message'] = $e->getMessage();
    }
} else {
    http_response_code(405);
    $response['message'] = 'Method Not Allowed';
}

echo json_encode($response);
?>