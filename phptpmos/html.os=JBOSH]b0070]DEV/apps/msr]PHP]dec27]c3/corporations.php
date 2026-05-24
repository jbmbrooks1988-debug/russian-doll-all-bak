<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MSR :: Corporations</title>
    <link rel="stylesheet" href="css/terminal.css">
</head>
<body>
    <div id="game-container">
        <h1>CORPORATIONS</h1>
        <div class="separator">================================================</div>
        <a href="index.html"><button>Back to Control Panel</button></a>
        
        <table style="margin-top: 20px;">
            <thead>
                <tr>
                    <th>TICKER</th>
                    <th>NAME</th>
                    <th>INDUSTRY</th>
                    <th>STOCK PRICE</th>
                </tr>
            </thead>
            <tbody>
                <?php
                const PHP_GENERATED_CORPORATIONS_BASE_PATH = 'data/corporations/generated/';

                if (!is_dir(PHP_GENERATED_CORPORATIONS_BASE_PATH)) {
                    echo '<tr><td colspan="4">Corporations directory not found. Please run setup.</td></tr>';
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
                        
                        $name = 'N/A';
                        $industry = 'N/A';
                        $price = 'N/A';

                        // Read details.txt
                        if (file_exists($details_file)) {
                            $details_content = file($details_file, FILE_IGNORE_NEW_LINES);
                            foreach($details_content as $line) {
                                if (strpos($line, 'Name:') === 0) {
                                    $name = trim(substr($line, 5));
                                }
                                if (strpos($line, 'Industry:') === 0) {
                                    $industry = trim(substr($line, 9));
                                }
                            }
                        }

                        // Read stock_data.txt
                        if (file_exists($stock_data_file)) {
                            $stock_content = file_get_contents($stock_data_file);
                            if (preg_match('/stock_price:\s*([0-9\.]+)/', $stock_content, $matches)) {
                                $price = '$' . $matches[1];
                            }
                        }

                        echo '<tr>';
                        echo '<td>' . htmlspecialchars($corp_ticker) . '</td>';
                        echo '<td>' . htmlspecialchars($name) . '</td>';
                        echo '<td>' . htmlspecialchars($industry) . '</td>';
                        echo '<td>' . htmlspecialchars($price) . '</td>';
                        echo '</tr>';
                    }
                }
                ?>
            </tbody>
        </table>
        
        <a href="index.html" style="margin-top: 20px; display: inline-block;"><button>Back to Control Panel</button></a>
    </div>
</body>
</html>