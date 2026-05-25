<?php
/**
 * Vocabulary creation functions for PHP LLM Backend
 */
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/helpers.php';

/**
 * Handle vocabulary creation requests
 */
function handleVocabRequest() {
    validateRequestMethod('POST');
    
    $data = getJsonFromBody();
    
    // Extract parameters with defaults
    $source = $data['source'] ?? '';
    $vocabName = $data['vocabName'] ?? 'default';
    
    if (empty($source)) {
        sendJsonResponse([
            'status' => 'error',
            'message' => 'Source file path is required'
        ], 400);
    }
    
    try {
        // Validate source path
        $sourcePath = DATA_PATH . $source;
        if (!file_exists($sourcePath)) {
            throw new Exception("Source file not found: $sourcePath");
        }
        
        // Load the corpus text
        $text = loadTextData($sourcePath);
        
        // Create vocabulary
        $vocab = createVocabulary($text);
        
        // Save vocabulary to file
        $vocabFilename = VOCAB_SAVE_PATH . $vocabName . '.json';
        if (!is_dir(VOCAB_SAVE_PATH)) {
            mkdir(VOCAB_SAVE_PATH, 0755, true);
        }
        
        $vocabData = [
            'vocab' => $vocab,
            'metadata' => [
                'name' => $vocabName,
                'size' => count($vocab),
                'created_at' => date('c'),
                'source' => $source
            ]
        ];
        
        $result = file_put_contents($vocabFilename, json_encode($vocabData, JSON_PRETTY_PRINT));
        if ($result === false) {
            throw new Exception("Could not save vocabulary file: $vocabFilename");
        }
        
        sendJsonResponse([
            'status' => 'success',
            'message' => 'Vocabulary created successfully',
            'vocab' => [
                'name' => $vocabName,
                'size' => count($vocab),
                'source' => $source,
                'path' => $vocabFilename
            ]
        ]);
    } catch (Exception $e) {
        logMessage("Vocabulary creation error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Load vocabulary from file
 */
function loadVocabulary($vocabName = 'default') {
    $vocabPath = VOCAB_SAVE_PATH . $vocabName . '.json';
    
    if (!file_exists($vocabPath)) {
        throw new Exception("Vocabulary file not found: $vocabPath");
    }
    
    $content = file_get_contents($vocabPath);
    if ($content === false) {
        throw new Exception("Could not read vocabulary file: $vocabPath");
    }
    
    $vocabData = json_decode($content, true);
    if ($vocabData === null) {
        throw new Exception("Could not decode vocabulary file: $vocabPath");
    }
    
    return $vocabData['vocab'];
}

/**
 * Get vocabulary information
 */
function handleGetVocabRequest($vocabName = 'default') {
    validateRequestMethod('GET');
    
    try {
        $vocabPath = VOCAB_SAVE_PATH . $vocabName . '.json';
        
        if (!file_exists($vocabPath)) {
            sendJsonResponse([
                'status' => 'error',
                'message' => "Vocabulary not found: $vocabName"
            ], 404);
            return;
        }
        
        $content = file_get_contents($vocabPath);
        $vocabData = json_decode($content, true);
        
        sendJsonResponse([
            'status' => 'success',
            'vocab' => [
                'name' => $vocabName,
                'size' => count($vocabData['vocab']),
                'metadata' => $vocabData['metadata'] ?? []
            ]
        ]);
    } catch (Exception $e) {
        logMessage("Get vocabulary error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

// Define vocabulary save path if not already defined
if (!defined('VOCAB_SAVE_PATH')) {
    define('VOCAB_SAVE_PATH', __DIR__ . '/../data/vocab/');
}