<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MSR :: Governments</title>
    <link rel="stylesheet" href="css/terminal.css">
</head>
<body>
    <div id="game-container">
        <h1>GOVERNMENTS</h1>
        <div class="separator">================================================</div>
        <a href="index.html"><button>Back to Control Panel</button></a>

        <table style="margin-top: 20px;">
            <thead>
                <tr>
                    <th>GOVERNMENT</th>
                    <th>POPULATION</th>
                    <th>GDP (Martian Credits)</th>
                </tr>
            </thead>
            <tbody>
                <?php
                const PHP_GENERATED_GOV_BASE_PATH = 'data/governments/generated/';

                if (!is_dir(PHP_GENERATED_GOV_BASE_PATH)) {
                    echo '<tr><td colspan="3">Governments directory not found. Please run setup.</td></tr>';
                    exit;
                }

                $gov_dirs = scandir(PHP_GENERATED_GOV_BASE_PATH);

                foreach ($gov_dirs as $gov_dir_name) {
                    if ($gov_dir_name === '.' || $gov_dir_name === '..') {
                        continue;
                    }

                    $gov_dir_path = PHP_GENERATED_GOV_BASE_PATH . $gov_dir_name . '/';
                    if (is_dir($gov_dir_path)) {
                        $data_file = $gov_dir_path . $gov_dir_name . '.txt';
                        
                        $name = $gov_dir_name; // Fallback to directory name
                        $population = 'N/A';
                        $gdp = 'N/A';

                        if (file_exists($data_file)) {
                            $data_content = file($data_file, FILE_IGNORE_NEW_LINES);
                            foreach($data_content as $line) {
                                if (strpos($line, 'GOVERNMENT FINANCIAL DATA:') === 0) {
                                    $name = trim(substr($line, 28));
                                }
                                if (strpos($line, 'Population:') === 0) {
                                    $population = trim(substr($line, 11));
                                }
                                if (strpos($line, 'GDP:') === 0) {
                                    $gdp = trim(substr($line, 4));
                                }
                            }
                        }

                        echo '<tr>';
                        echo '<td>' . htmlspecialchars($name) . '</td>';
                        echo '<td>' . htmlspecialchars($population) . '</td>';
                        echo '<td>' . htmlspecialchars($gdp) . '</td>';
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