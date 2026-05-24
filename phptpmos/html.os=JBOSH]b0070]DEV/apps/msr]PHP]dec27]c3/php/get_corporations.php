<?php
header('Content-Type: application/json');

const PHP_GENERATED_CORPORATIONS_BASE_PATH = '../data/corporations/generated/';

$corporations = [];

if (!is_dir(PHP_GENERATED_CORPORATIONS_BASE_PATH)) {
    echo json_encode(['error' => 'Corporations directory not found.']);
    exit;
}

$corporation_dirs = scandir(PHP_GENERATED_CORPORATIONS_BASE_PATH);

foreach ($corporation_dirs as $corp_ticker) {
    if ($corp_ticker === '.' || $corp_ticker === '..') {
        continue;
    }

    $corp_dir_path = PHP_GENERATED_CORPORATIONS_BASE_PATH . $corp_ticker . '/';
    if (is_dir($corp_dir_path)) {
        $details_file = $corp_dir_path . 'details.txt';
        $stock_data_file = $corp_dir_path . 'stock_data.txt';
        
        $corp_data = [
            'ticker' => $corp_ticker,
            'name' => 'N/A',
            'industry' => 'N/A',
            'stock_price' => 'N/A'
        ];

        // Read details.txt
        if (file_exists($details_file)) {
            $details_content = file($details_file, FILE_IGNORE_NEW_LINES);
            foreach($details_content as $line) {
                if (strpos($line, 'Name:') === 0) {
                    $corp_data['name'] = trim(substr($line, 5));
                }
                if (strpos($line, 'Industry:') === 0) {
                    $corp_data['industry'] = trim(substr($line, 9));
                }
            }
        }

        // Read stock_data.txt
        if (file_exists($stock_data_file)) {
            $stock_content = file_get_contents($stock_data_file);
            if (preg_match('/stock_price:\s*([0-9\.]+)/', $stock_content, $matches)) {
                $corp_data['stock_price'] = (float)$matches[1];
            }
        }

        $corporations[] = $corp_data;
    }
}

echo json_encode($corporations);
?>