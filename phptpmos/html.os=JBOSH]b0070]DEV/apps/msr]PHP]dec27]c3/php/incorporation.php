<?php
header('Content-Type: application/json');

// --- Helper Functions ---
function str_replace_first($from, $to, $subject) {
    $from = '/'.preg_quote($from, '/').'/';
    return preg_replace($from, $to, $subject, 1);
}

// --- Receive and Validate Data ---
$corp_name = isset($_POST['corp_name']) ? trim($_POST['corp_name']) : '';
$ticker = isset($_POST['ticker']) ? trim(strtoupper($_POST['ticker'])) : '';
$industry = isset($_POST['industry']) ? trim($_POST['industry']) : '';
$country = isset($_POST['country']) ? trim($_POST['country']) : '';
$funding_amount = isset($_POST['funding_amount']) ? (int)$_POST['funding_amount'] : 0;
$player_name = isset($_POST['player_name']) ? preg_replace('/[^a-zA-Z0-9_-]/', '', $_POST['player_name']) : '';

if (empty($corp_name) || empty($ticker) || empty($industry) || empty($country) || $funding_amount <= 0 || empty($player_name)) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'All fields are required.']);
    exit;
}

// --- Paths ---
$corp_dir = '../data/corporations/generated/' . $ticker;
$new_corp_path = $corp_dir . '/' . $ticker . '.txt';
$shareholders_path = $corp_dir . '/list_shareholders.txt';
$martian_corps_path = '../data/corporations/generated/martian_corporations.txt';
$portfolio_path = '../data/players/' . $player_name . '/portfolio.txt';
$ledger_path = '../data/master_ledger.txt';

try {
    // --- Validation ---
    if (is_dir($corp_dir)) {
        throw new Exception("Ticker symbol already exists.");
    }

    // --- Select Template ---
    // Corrected paths to templates
    $template_path_base = '../data/templates/';
    $template_file = 'corporation_example.txt';
    if ($industry === 'BANKING') {
        $template_file = 'bank_example.txt';
    } elseif ($industry === 'INSURANCE') {
        $template_file = 'insurance_example.txt';
    }
    $full_template_path = $template_path_base . $template_file;
    
    $template_content = file_get_contents($full_template_path);
    if ($template_content === false) {
        throw new Exception("Could not read corporation template file: " . $full_template_path);
    }

    // --- Create Directory ---
    mkdir($corp_dir, 0777, true);

    // --- Inject Corporation Data into Template ---
    $new_corp_content = $template_content;

    // Replace Name and Ticker in headers
    $new_corp_content = preg_replace(
        '/FINANCIAL PROFILE FOR\s+[^(\r\n]+?\s+\([^)]+\)/',
        "FINANCIAL PROFILE FOR $corp_name ($ticker)",
        $new_corp_content,
        1
    );
    $new_corp_content = preg_replace(
        '/FINANCIAL BALANCE SHEET:\s+[^(\r\n]+?\s+\([^)]+\)/',
        "FINANCIAL BALANCE SHEET: $corp_name ($ticker)",
        $new_corp_content,
        1
    );

    // Replace Industry Group
    $new_corp_content = preg_replace('/(Industry Group:\s+)(.*)/', "$1$industry", $new_corp_content, 1);
    // Replace Incorporated in
    $new_corp_content = preg_replace('/(Incorporated in:\s+)(.*)/', "$1$country", $new_corp_content, 1);
    // Replace Controlled by and Wholly owned by (simplified for now to player_name)
    $new_corp_content = preg_replace('/(Controlled \(and actively managed\) by:\s+)(.*)/', "$1$player_name", $new_corp_content, 1);
    $new_corp_content = preg_replace('/(Wholly owned by:\s+)(.*)/', "$1$player_name", $new_corp_content, 1);
    
    // Replace dynamic financial values based on funding amount
    $formatted_funding_amount = number_format($funding_amount, 2, '.', ''); // Ensure 2 decimal places, no thousands separator
    
    // Pattern to find lines with key: value and update the value part
    $patterns = [
        '/(Free Cash and Equivalents:\s+)([0-9\.\-]+)/' => $formatted_funding_amount,
        '/(Total Assets:\s+)([0-9\.\-]+)/' => $formatted_funding_amount,
        '/(Equity \(Net Worth\):\s+)([0-9\.\-]+)/' => $formatted_funding_amount,
        '/(Net Worth Per Share:\s+)([0-9\.\-]+)/' => $formatted_funding_amount, // Simplified, should be calculated
        '/(Current Stock Price:\s+)([0-9\.\-]+)/' => $formatted_funding_amount, // Simplified, should be calculated
        '/(Total Stock Capitalization:\s+)([0-9\.\-]+)/' => $formatted_funding_amount, // Simplified, should be calculated
        // For banks specifically
        '/(Demand Deposits:\s+)([0-9\.\-]+)/' => $formatted_funding_amount,
        '/(Certificates of Deposit:\s+)([0-9\.\-]+)/' => $formatted_funding_amount,
        '/(Total Liabilities:\s+)([0-9\.\-]+)/' => '0.00', // Assuming new corp has no liabilities initially
    ];

    foreach ($patterns as $pattern => $replacement_value) {
        $new_corp_content = preg_replace($pattern, "$1" . $replacement_value, $new_corp_content, 1);
    }
    
    file_put_contents($new_corp_path, $new_corp_content);

    // --- Create Shareholders File ---
    $shareholders_content = "CORPORATE STOCKHOLDER RECORDS FOR $corp_name ($ticker)\n";
    $shareholders_content .= "Incorporated in: $country\n";
    $shareholders_content .= "-------------------------------\n";
    $shareholders_content .= "LIST OF SHAREHOLDERS OF $corp_name ($ticker):\n";
    $shareholders_content .= "------------------------------------------------------------\n";
    $shareholders_content .= "$player_name\n";
    $shareholders_content .= "100%\n";
    $shareholders_content .= "--------------------------------\n";
    file_put_contents($shareholders_path, $shareholders_content);

    // --- Append to martian_corporations.txt ---
    $martian_line = "$corp_name ($ticker) Ind: $industry\n";
    file_put_contents($martian_corps_path, $martian_line, FILE_APPEND);
    
    // --- Update Player's Portfolio ---
    $portfolio_content = file_get_contents($portfolio_path);
    if ($portfolio_content === false) {
        // Create initial portfolio content if file doesn't exist
        $upper_player_name = strtoupper($player_name);
        $portfolio_content = "STOCK & BOND PORTFOLIO LISTING FOR " . $upper_player_name . "\n";
        $portfolio_content .= "STOCK PORTFOLIO FOR " . $upper_player_name . " (In U.S. Dollars)\n";
        $portfolio_content .= "-----------------------------------------------------------------------------------\n";
        $portfolio_content .= "STOCK PCT% SHARE DIV. VALUE: ANALYST'S\n";
        $portfolio_content .= "COMPANY SYM. HELD PRICE YIELD (MILLIONS) RATING\n";
        $portfolio_content .= "---------------------------- ----- ---- -------- ----- ------------- -------------\n";
        $portfolio_content .= "$player_name OWNS NO BONDS ....\n"; // Placeholder for bonds
        $portfolio_content .= "-----------------------------------------------------------------------------------\n";
    }

    $stock_entry_line = sprintf("%-28s %-5s 100%% %8.2f 0.0%% %13s NOT TRADED",
                                $corp_name, $ticker, $funding_amount / 100, $formatted_funding_amount);

    // Find the insertion point for new stock entries (before TOTAL STOCK PORTFOLIO VALUE)
    $portfolio_content = preg_replace(
        '/(---------------------------- ----- ---- -------- ----- ------------- -------------\s*\\nTOTAL STOCK PORTFOLIO VALUE \(\$ Million\): )([0-9,\.]+)/',
        "$stock_entry_line\n$1" . ($funding_amount + (float)str_replace(',', '', '$2')),
        $portfolio_content,
        1,
        $count
    );

    if ($count == 0) {
        // If the pattern wasn't found (e.g., first stock), append in the right section
        $portfolio_content = preg_replace(
            '/(---------------------------- ----- ---- -------- ----- ------------- -------------\s*\\n)/',
            "$1$stock_entry_line\n",
            $portfolio_content,
            1
        );
        // And if the total line isn't there, just append it
        if (strpos($portfolio_content, 'TOTAL STOCK PORTFOLIO VALUE') === false) {
            $portfolio_content .= "TOTAL STOCK PORTFOLIO VALUE (\$ Million): $formatted_funding_amount\n";
        }
    }
    
    // Update total net worth for the player
    $player_balance_sheet_path = '../data/players/' . $player_name . '/balance_sheet.txt';
    $balance_sheet_content = file_get_contents($player_balance_sheet_path);
    if ($balance_sheet_content !== false) {
        // Update total assets and net worth based on new portfolio value
        // This is a simplified approach, a full balance sheet calculation would be complex
        $balance_sheet_content = preg_replace_callback(
            '/(Stock Portfolio\.\.\.\.\.\.\s*)([0-9\.\-]+)/',
            function ($m) use ($funding_amount) {
                return $m[1] . number_format((float)$m[2] + $funding_amount, 2);
            },
            $balance_sheet_content
        );
        $balance_sheet_content = preg_replace_callback(
            '/(Total Assets\.\.\.\.\.\.\.\s*)([0-9\.\-]+)/',
            function ($m) use ($funding_amount) {
                return $m[1] . number_format((float)$m[2] + $funding_amount, 2);
            },
            $balance_sheet_content
        );
        $balance_sheet_content = preg_replace_callback(
            '/(Net Worth\.\.\.\.\.\.\.\.\s*)([0-9\.\-]+)/',
            function ($m) use ($funding_amount) {
                return $m[1] . number_format((float)$m[2] + $funding_amount, 2);
            },
            $balance_sheet_content
        );
        file_put_contents($player_balance_sheet_path, $balance_sheet_content);
    }

    file_put_contents($portfolio_path, $portfolio_content);


    // --- Append to Master Ledger ---
    $ledger_line = date('Y-m-d H:i:s') . " | $ticker" . "_cash | " . $player_name . "_cash | " . $formatted_funding_amount . " | Company creation\n";
    file_put_contents($ledger_path, $ledger_line, FILE_APPEND);

    echo json_encode(['status' => 'success', 'message' => "Successfully founded $corp_name."]);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}
?>
