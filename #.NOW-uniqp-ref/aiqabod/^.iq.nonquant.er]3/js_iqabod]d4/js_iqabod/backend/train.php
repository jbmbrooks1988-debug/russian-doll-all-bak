<?php
/**
 * Training functionality for PHP LLM Backend
 */
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/helpers.php';

class LLMTrainer {
    private $vocab;
    private $modelData;
    private $trainingData;
    
    public function __construct() {
        $this->modelData = [];
        $this->trainingData = [];
    }
    
    /**
     * Load training data from corpora directory
     */
    public function loadTrainingData($corporaPath = CORPORA_PATH) {
        if (!is_dir($corporaPath)) {
            throw new Exception("Corpora directory does not exist: $corporaPath");
        }
        
        $files = scandir($corporaPath);
        $textData = '';
        
        foreach ($files as $file) {
            if (pathinfo($file, PATHINFO_EXTENSION) === 'txt') {
                $filePath = $corporaPath . $file;
                $content = loadTextData($filePath);
                $textData .= $content . ' ';
            }
        }
        
        if (empty($textData)) {
            throw new Exception("No training data found in corpora directory: $corporaPath");
        }
        
        // Create vocabulary from the text data
        $this->vocab = createVocabulary($textData);
        
        // Tokenize the training data
        $tokens = tokenize($textData);
        $tokenIds = tokensToIds($tokens, $this->vocab);
        
        // Create training sequences (context windows)
        for ($i = 0; $i < count($tokenIds) - CONTEXT_LENGTH; $i += CONTEXT_LENGTH) {
            $sequence = array_slice($tokenIds, $i, CONTEXT_LENGTH + 1);
            $this->trainingData[] = $sequence;
        }
        
        logMessage("Loaded " . count($this->trainingData) . " training sequences");
        return true;
    }
    
    /**
     * Initialize model weights
     */
    public function initializeModel() {
        logMessage("Initializing model weights...");
        
        // Initialize embedding layer
        $this->modelData['embedding'] = [];
        for ($i = 0; $i < MODEL_VOCAB_SIZE; $i++) {
            $this->modelData['embedding'][$i] = [];
            for ($j = 0; $j < MODEL_EMBED_DIM; $j++) {
                // Initialize with small random values
                $this->modelData['embedding'][$i][$j] = (mt_rand() / mt_getrandmax() - 0.5) * 0.1;
            }
        }
        
        // Initialize transformer blocks
        $this->modelData['transformer_blocks'] = [];
        for ($layer = 0; $layer < MODEL_NUM_LAYERS; $layer++) {
            $this->modelData['transformer_blocks'][$layer] = [
                'attention' => [
                    'query_weights' => $this->randomMatrix(MODEL_EMBED_DIM, MODEL_EMBED_DIM),
                    'key_weights' => $this->randomMatrix(MODEL_EMBED_DIM, MODEL_EMBED_DIM),
                    'value_weights' => $this->randomMatrix(MODEL_EMBED_DIM, MODEL_EMBED_DIM),
                    'output_weights' => $this->randomMatrix(MODEL_EMBED_DIM, MODEL_EMBED_DIM),
                ],
                'ffn' => [
                    'dense1_weights' => $this->randomMatrix(MODEL_EMBED_DIM, MODEL_FF_DIM),
                    'dense1_bias' => array_fill(0, MODEL_FF_DIM, 0),
                    'dense2_weights' => $this->randomMatrix(MODEL_FF_DIM, MODEL_EMBED_DIM),
                    'dense2_bias' => array_fill(0, MODEL_EMBED_DIM, 0),
                ],
                'norm1_weights' => array_fill(0, MODEL_EMBED_DIM, 1),
                'norm1_bias' => array_fill(0, MODEL_EMBED_DIM, 0),
                'norm2_weights' => array_fill(0, MODEL_EMBED_DIM, 1),
                'norm2_bias' => array_fill(0, MODEL_EMBED_DIM, 0),
            ];
        }
        
        // Initialize final layer norm and output projection
        $this->modelData['final_norm_weights'] = array_fill(0, MODEL_EMBED_DIM, 1);
        $this->modelData['final_norm_bias'] = array_fill(0, MODEL_EMBED_DIM, 0);
        $this->modelData['output_projection'] = $this->randomMatrix(MODEL_EMBED_DIM, MODEL_VOCAB_SIZE);
        
        logMessage("Model initialized with " . $this->getModelSize() . " parameters");
    }
    
    /**
     * Generate random matrix
     */
    private function randomMatrix($rows, $cols) {
        $matrix = [];
        for ($i = 0; $i < $rows; $i++) {
            $matrix[$i] = [];
            for ($j = 0; $j < $cols; $j++) {
                $matrix[$i][$j] = (mt_rand() / mt_getrandmax() - 0.5) * 0.1;
            }
        }
        return $matrix;
    }
    
    /**
     * Get total model size (number of parameters)
     */
    private function getModelSize() {
        $total = 0;
        
        // Embedding layer
        $total += MODEL_VOCAB_SIZE * MODEL_EMBED_DIM;
        
        // Transformer blocks
        $total += MODEL_NUM_LAYERS * (
            (MODEL_EMBED_DIM * MODEL_EMBED_DIM) * 3 + // Q, K, V weights
            (MODEL_EMBED_DIM * MODEL_EMBED_DIM) +     // Output weights
            (MODEL_EMBED_DIM * MODEL_FF_DIM) +        // FFN dense1 weights
            MODEL_FF_DIM +                            // FFN dense1 bias
            (MODEL_FF_DIM * MODEL_EMBED_DIM) +        // FFN dense2 weights
            MODEL_EMBED_DIM +                         // FFN dense2 bias
            MODEL_EMBED_DIM * 4                       // Normalization parameters (weight and bias for 2 norms each)
        );
        
        // Final layer norm and output projection
        $total += MODEL_EMBED_DIM * 2; // Final norm weight and bias
        $total += MODEL_EMBED_DIM * MODEL_VOCAB_SIZE; // Output projection
        
        return $total;
    }
    
    /**
     * Perform model training
     */
    public function train($epochs = TRAIN_EPOCHS, $batchSize = TRAIN_BATCH_SIZE, $learningRate = TRAIN_LEARNING_RATE) {
        logMessage("Starting training for $epochs epochs with batch size $batchSize and learning rate $learningRate");
        
        $totalBatches = ceil(count($this->trainingData) / $batchSize);
        
        for ($epoch = 0; $epoch < $epochs; $epoch++) {
            logMessage("Epoch $epoch/{$epochs}");
            
            $epochLoss = 0;
            $batchesProcessed = 0;
            
            // Process in batches
            for ($i = 0; $i < count($this->trainingData); $i += $batchSize) {
                $batch = array_slice($this->trainingData, $i, $batchSize);
                
                // Calculate loss for the batch
                $batchLoss = $this->calculateBatchLoss($batch);
                $epochLoss += $batchLoss;
                
                // Update model parameters using gradients (simplified)
                $this->updateParameters($batch, $learningRate);
                
                $batchesProcessed++;
                
                // Log progress
                if ($batchesProcessed % TRAIN_VALIDATE_EVERY === 0) {
                    logMessage("Processed {$batchesProcessed}/{$totalBatches} batches, current average loss: " . ($epochLoss / $batchesProcessed));
                }
            }
            
            $avgEpochLoss = $epochLoss / max(1, $batchesProcessed);
            logMessage("Completed epoch $epoch with average loss: $avgEpochLoss");
        }
        
        logMessage("Training completed");
    }
    
    /**
     * Calculate loss for a batch of training data
     */
    private function calculateBatchLoss($batch) {
        $totalLoss = 0;
        $count = 0;
        
        foreach ($batch as $sequence) {
            // Skip sequence if it's too short
            if (count($sequence) < 2) continue;
            
            // In a real implementation, we would perform a forward pass
            // and calculate cross-entropy loss
            // For this simulation, return a random loss value
            $loss = 2.0 - (0.1 * mt_rand() / mt_getrandmax()); // Simulated loss between 1.9 and 2.0
            $totalLoss += $loss;
            $count++;
        }
        
        return $count > 0 ? $totalLoss / $count : 0;
    }
    
    /**
     * Update model parameters using gradients
     */
    private function updateParameters($batch, $learningRate) {
        // In a real implementation, we would calculate gradients and update weights
        // For this simulation, we'll just modify weights slightly
        foreach ($this->modelData['embedding'] as $tokenId => &$embeddings) {
            if (mt_rand() / mt_getrandmax() < 0.1) { // Only update 10% of embeddings randomly
                foreach ($embeddings as $dim => &$val) {
                    $val += ($mt_rand() / mt_getrandmax() - 0.5) * $learningRate * 0.01;
                }
            }
        }
    }
    
    /**
     * Save trained model
     */
    public function saveModel($modelName = 'default') {
        $modelPath = MODEL_SAVE_PATH . $modelName . '.json';
        
        $modelData = [
            'vocab' => $this->vocab,
            'model' => $this->modelData,
            'config' => [
                'vocab_size' => MODEL_VOCAB_SIZE,
                'embed_dim' => MODEL_EMBED_DIM,
                'num_heads' => MODEL_NUM_HEADS,
                'ff_dim' => MODEL_FF_DIM,
                'num_layers' => MODEL_NUM_LAYERS,
                'context_length' => CONTEXT_LENGTH
            ]
        ];
        
        saveModelData($modelData, $modelPath);
        logMessage("Model saved to: $modelPath");
    }
    
    /**
     * Load model from file
     */
    public function loadModel($modelName = 'default') {
        $modelPath = MODEL_SAVE_PATH . $modelName . '.json';
        $modelData = loadModelData($modelPath);
        
        $this->vocab = $modelData['vocab'];
        $this->modelData = $modelData['model'];
        
        logMessage("Model loaded from: $modelPath");
    }
}

// Example usage when running the script directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    try {
        // Create trainer instance
        $trainer = new LLMTrainer();
        
        // Load training data
        $trainer->loadTrainingData();
        
        // Initialize model
        $trainer->initializeModel();
        
        // Train model
        $trainer->train();
        
        // Save trained model
        $trainer->saveModel();
        
        echo "Training completed successfully!\n";
    } catch (Exception $e) {
        echo "Error during training: " . $e->getMessage() . "\n";
        logMessage("Training error: " . $e->getMessage(), 'ERROR');
    }
}