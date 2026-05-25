<?php
/**
 * REST API endpoints for PHP LLM Backend
 */
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/helpers.php';
require_once __DIR__ . '/train.php';
require_once __DIR__ . '/generate.php';
require_once __DIR__ . '/vocab.php';

// Enable CORS for frontend communication
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type, Authorization");

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

// Parse the request
$requestMethod = $_SERVER['REQUEST_METHOD'];
$pathInfo = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
$basePath = dirname($_SERVER['SCRIPT_NAME']);
$relativePath = substr($pathInfo, strlen($basePath));
if ($relativePath[0] === '/') {
    $relativePath = substr($relativePath, 1);
}

// Route the request to the appropriate handler
if ($relativePath === 'train' && $requestMethod === 'POST') {
    handleTrainRequest();
} elseif ($relativePath === 'generate' && $requestMethod === 'POST') {
    handleGenerateRequest();
} elseif ($relativePath === 'vocab' && $requestMethod === 'POST') {
    handleVocabRequest();
} elseif ($relativePath === 'vocab' && $requestMethod === 'GET') {
    $vocabName = $_GET['name'] ?? 'default';
    handleGetVocabRequest($vocabName);
} elseif ($relativePath === 'status' && $requestMethod === 'GET') {
    handleStatusRequest();
} elseif ($relativePath === 'config' && $requestMethod === 'POST') {
    handleConfigRequest();
} elseif ($relativePath === 'config' && $requestMethod === 'GET') {
    handleGetConfigRequest();
} elseif ($relativePath === 'corpus' && $requestMethod === 'GET') {
    $fileName = $_GET['name'] ?? 'sample.txt';
    handleGetCorpusFileRequest($fileName);
} elseif ($relativePath === 'vocab-info' && $requestMethod === 'GET') {
    $vocabName = $_GET['name'] ?? 'default';
    handleGetVocabInfoRequest($vocabName);
} elseif ($relativePath === 'model-info' && $requestMethod === 'GET') {
    $modelName = $_GET['name'] ?? 'default';
    handleGetModelInfoRequest($modelName);
} elseif ($relativePath === 'list-corpus' && $requestMethod === 'GET') {
    handleListCorpusFilesRequest();
} elseif ($relativePath === 'list-vocab' && $requestMethod === 'GET') {
    handleListVocabFilesRequest();
} elseif ($relativePath === 'list-model' && $requestMethod === 'GET') {
    handleListModelFilesRequest();
} else {
    sendJsonResponse(['error' => 'Endpoint not found'], 404);
}

/**
 * Handle training requests
 */
function handleTrainRequest() {
    validateRequestMethod('POST');
    
    $data = getJsonFromBody();
    
    // Extract parameters with defaults
    $epochs = (int)($data['epochs'] ?? TRAIN_EPOCHS);
    $batchSize = (int)($data['batchSize'] ?? TRAIN_BATCH_SIZE);
    $learningRate = (float)($data['learningRate'] ?? TRAIN_LEARNING_RATE);
    $modelName = $data['modelName'] ?? 'default';
    $corporaPath = $data['corporaPath'] ?? CORPORA_PATH;
    
    try {
        // Create trainer instance
        $trainer = new LLMTrainer();
        
        // Load training data
        $trainer->loadTrainingData($corporaPath);
        
        // Initialize model if not exists, otherwise load existing
        $modelPath = MODEL_SAVE_PATH . $modelName . '.json';
        if (file_exists($modelPath)) {
            $trainer->loadModel($modelName);
            logMessage("Continuing training for existing model: $modelName");
        } else {
            $trainer->initializeModel();
            logMessage("Starting training for new model: $modelName");
        }
        
        // Train model
        $trainer->train($epochs, $batchSize, $learningRate);
        
        // Save trained model
        $trainer->saveModel($modelName);
        
        sendJsonResponse([
            'status' => 'success',
            'message' => 'Training completed successfully',
            'model' => $modelName,
            'epochs' => $epochs,
            'batchSize' => $batchSize
        ]);
    } catch (Exception $e) {
        logMessage("Training error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle generation requests
 */
function handleGenerateRequest() {
    validateRequestMethod('POST');
    
    $data = getJsonFromBody();
    
    // Extract parameters with defaults
    $prompt = $data['prompt'] ?? '';
    $maxTokens = (int)($data['maxTokens'] ?? GENERATE_MAX_TOKENS);
    $temperature = (float)($data['temperature'] ?? GENERATE_TEMPERATURE);
    $topK = (int)($data['topK'] ?? GENERATE_TOP_K);
    $topP = (float)($data['topP'] ?? GENERATE_TOP_P);
    $modelName = $data['modelName'] ?? 'default';
    
    if (empty($prompt)) {
        sendJsonResponse([
            'status' => 'error',
            'message' => 'Prompt is required'
        ], 400);
    }
    
    try {
        // Create generator instance
        $generator = new LLMGenerator();
        
        // Load model
        $generator->loadModel($modelName);
        
        // Generate text
        $result = $generator->generate($prompt, $maxTokens, $temperature, $topK, $topP);
        
        sendJsonResponse([
            'status' => 'success',
            'prompt' => $prompt,
            'generated' => $result,
            'model' => $modelName,
            'maxTokens' => $maxTokens,
            'temperature' => $temperature,
            'topK' => $topK,
            'topP' => $topP
        ]);
    } catch (Exception $e) {
        logMessage("Generation error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle status requests
 */
function handleStatusRequest() {
    validateRequestMethod('GET');
    
    try {
        // Check if default model exists
        $modelPath = MODEL_SAVE_PATH . 'default.json';
        $modelExists = file_exists($modelPath);
        
        $status = [
            'status' => 'running',
            'timestamp' => date('c'),
            'model_loaded' => $modelExists,
            'model_path' => $modelPath,
            'config' => [
                'vocab_size' => MODEL_VOCAB_SIZE,
                'embed_dim' => MODEL_EMBED_DIM,
                'num_heads' => MODEL_NUM_HEADS,
                'ff_dim' => MODEL_FF_DIM,
                'num_layers' => MODEL_NUM_LAYERS,
                'context_length' => CONTEXT_LENGTH
            ]
        ];
        
        sendJsonResponse($status);
    } catch (Exception $e) {
        logMessage("Status error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle config update requests
 */
function handleConfigRequest() {
    validateRequestMethod('POST');
    
    $data = getJsonFromBody();
    
    // For now, just return the current config
    // In a real implementation, this would update runtime configuration
    
    sendJsonResponse([
        'status' => 'success',
        'message' => 'Config updated',
        'config' => $data
    ]);
}

/**
 * Handle getting config requests
 */
function handleGetConfigRequest() {
    validateRequestMethod('GET');
    
    sendJsonResponse([
        'status' => 'success',
        'config' => [
            'default' => [
                'vocab_size' => MODEL_VOCAB_SIZE,
                'embed_dim' => MODEL_EMBED_DIM,
                'num_heads' => MODEL_NUM_HEADS,
                'ff_dim' => MODEL_FF_DIM,
                'num_layers' => MODEL_NUM_LAYERS,
                'context_length' => CONTEXT_LENGTH,
                'train_batch_size' => TRAIN_BATCH_SIZE,
                'train_learning_rate' => TRAIN_LEARNING_RATE,
                'train_epochs' => TRAIN_EPOCHS,
                'generate_temperature' => GENERATE_TEMPERATURE,
                'generate_top_k' => GENERATE_TOP_K,
                'generate_top_p' => GENERATE_TOP_P,
                'generate_max_tokens' => GENERATE_MAX_TOKENS
            ]
        ]
    ]);
}

/**
 * Handle getting corpus file request
 */
function handleGetCorpusFileRequest($fileName) {
    validateRequestMethod('GET');
    
    try {
        // Validate file name to prevent directory traversal
        $fileName = basename($fileName); // Get only the filename without path
        $filePath = CORPORA_PATH . $fileName;
        
        if (!file_exists($filePath)) {
            sendJsonResponse([
                'status' => 'error',
                'message' => "Corpus file not found: $fileName"
            ], 404);
            return;
        }
        
        $content = file_get_contents($filePath);
        if ($content === false) {
            throw new Exception("Could not read corpus file: $fileName");
        }
        
        // For large files, we might want to truncate or implement chunking
        $maxSize = 10240; // 10KB max for display in terminal
        if (strlen($content) > $maxSize) {
            $truncated = substr($content, 0, $maxSize);
            $content = $truncated . "... (truncated, file too large to display in full)";
        }
        
        sendJsonResponse([
            'status' => 'success',
            'file' => $fileName,
            'size' => filesize($filePath),
            'content' => $content,
            'path' => $filePath
        ]);
    } catch (Exception $e) {
        logMessage("Get corpus file error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle getting vocabulary information request
 */
function handleGetVocabInfoRequest($vocabName) {
    validateRequestMethod('GET');
    
    try {
        // Validate vocab name to prevent directory traversal
        $vocabName = preg_replace('/[^a-zA-Z0-9_-]/', '', $vocabName);
        $vocabPath = VOCAB_SAVE_PATH . $vocabName . '.json';
        
        if (!file_exists($vocabPath)) {
            sendJsonResponse([
                'status' => 'error',
                'message' => "Vocabulary file not found: $vocabName"
            ], 404);
            return;
        }
        
        $content = file_get_contents($vocabPath);
        $vocabData = json_decode($content, true);
        
        if ($vocabData === null) {
            throw new Exception("Could not decode vocabulary file: $vocabName");
        }
        
        // Get the first 10 tokens as a sample
        $tokens = array_keys($vocabData['vocab']);
        $sampleTokens = array_slice($tokens, 0, 10);
        
        sendJsonResponse([
            'status' => 'success',
            'vocab' => [
                'name' => $vocabName,
                'size' => count($vocabData['vocab']),
                'metadata' => $vocabData['metadata'] ?? [],
                'tokens' => $sampleTokens
            ]
        ]);
    } catch (Exception $e) {
        logMessage("Get vocabulary info error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle getting model information request
 */
function handleGetModelInfoRequest($modelName) {
    validateRequestMethod('GET');
    
    try {
        // Validate model name to prevent directory traversal
        $modelName = preg_replace('/[^a-zA-Z0-9_-]/', '', $modelName);
        $modelPath = MODEL_SAVE_PATH . $modelName . '.json';
        
        $exists = file_exists($modelPath);
        
        $modelInfo = [
            'name' => $modelName,
            'exists' => $exists,
            'path' => $modelPath
        ];
        
        if ($exists) {
            $modelInfo['size'] = filesize($modelPath);
            $modelInfo['modified'] = date('c', filemtime($modelPath));
            $modelInfo['config'] = [
                'vocab_size' => MODEL_VOCAB_SIZE,
                'embed_dim' => MODEL_EMBED_DIM,
                'num_heads' => MODEL_NUM_HEADS,
                'ff_dim' => MODEL_FF_DIM,
                'num_layers' => MODEL_NUM_LAYERS,
                'context_length' => CONTEXT_LENGTH
            ];
        }
        
        sendJsonResponse([
            'status' => 'success',
            'model' => $modelInfo
        ]);
    } catch (Exception $e) {
        logMessage("Get model info error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle listing corpus files request
 */
function handleListCorpusFilesRequest() {
    validateRequestMethod('GET');
    
    try {
        $files = [];
        
        if (is_dir(CORPORA_PATH)) {
            $dir = opendir(CORPORA_PATH);
            if ($dir) {
                while (($file = readdir($dir)) !== false) {
                    if ($file !== '.' && $file !== '..' && is_file(CORPORA_PATH . $file)) {
                        $filePath = CORPORA_PATH . $file;
                        $files[] = [
                            'name' => $file,
                            'size' => filesize($filePath),
                            'modified' => date('c', filemtime($filePath))
                        ];
                    }
                }
                closedir($dir);
            }
        }
        
        sendJsonResponse([
            'status' => 'success',
            'files' => $files
        ]);
    } catch (Exception $e) {
        logMessage("List corpus files error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle listing vocabulary files request
 */
function handleListVocabFilesRequest() {
    validateRequestMethod('GET');
    
    try {
        $files = [];
        
        if (is_dir(VOCAB_SAVE_PATH)) {
            $dir = opendir(VOCAB_SAVE_PATH);
            if ($dir) {
                while (($file = readdir($dir)) !== false) {
                    if ($file !== '.' && $file !== '..' && is_file(VOCAB_SAVE_PATH . $file) && pathinfo($file, PATHINFO_EXTENSION) === 'json') {
                        $filePath = VOCAB_SAVE_PATH . $file;
                        $files[] = [
                            'name' => pathinfo($file, PATHINFO_FILENAME), // Get name without extension
                            'size' => filesize($filePath),
                            'modified' => date('c', filemtime($filePath))
                        ];
                    }
                }
                closedir($dir);
            }
        }
        
        sendJsonResponse([
            'status' => 'success',
            'files' => $files
        ]);
    } catch (Exception $e) {
        logMessage("List vocab files error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}

/**
 * Handle listing model files request
 */
function handleListModelFilesRequest() {
    validateRequestMethod('GET');
    
    try {
        $files = [];
        
        if (is_dir(MODEL_SAVE_PATH)) {
            $dir = opendir(MODEL_SAVE_PATH);
            if ($dir) {
                while (($file = readdir($dir)) !== false) {
                    if ($file !== '.' && $file !== '..' && is_file(MODEL_SAVE_PATH . $file) && pathinfo($file, PATHINFO_EXTENSION) === 'json') {
                        $filePath = MODEL_SAVE_PATH . $file;
                        $files[] = [
                            'name' => pathinfo($file, PATHINFO_FILENAME), // Get name without extension
                            'size' => filesize($filePath),
                            'modified' => date('c', filemtime($filePath))
                        ];
                    }
                }
                closedir($dir);
            }
        }
        
        sendJsonResponse([
            'status' => 'success',
            'files' => $files
        ]);
    } catch (Exception $e) {
        logMessage("List model files error: " . $e->getMessage(), 'ERROR');
        sendJsonResponse([
            'status' => 'error',
            'message' => $e->getMessage()
        ], 500);
    }
}