<?php
header('Content-Type: application/json');
include_once __DIR__ . '/gamestate_utils.php';

$response = ['status' => 'error', 'message' => 'An unknown error occurred.'];

try {
    $settings = read_settings(); // Read settings from data/setting.txt

    // Update Ticker Speed if provided
    if (isset($_POST['tickerSpeed'])) {
        $valid_speeds = ['sec', 'min', 'hour', 'day'];
        if (in_array($_POST['tickerSpeed'], $valid_speeds)) {
            $settings['ticker_speed'] = $_POST['tickerSpeed'];
            $response['message'] = 'Ticker speed updated.';
        } else {
            throw new Exception("Invalid ticker speed provided.");
        }
    } else {
        throw new Exception("No settings to update provided.");
    }

    // Save the updated settings to data/setting.txt
    write_settings($settings);

    $response['status'] = 'success';

} catch (Exception $e) {
    http_response_code(500);
    $response = ['status' => 'error', 'message' => $e->getMessage()];
}

echo json_encode($response);
?>
