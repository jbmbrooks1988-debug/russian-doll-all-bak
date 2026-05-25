<?php
/**
 * Configuration file for PHP LLM Backend
 */

// Model Configuration
define('MODEL_VOCAB_SIZE', 10000);
define('MODEL_EMBED_DIM', 128);
define('MODEL_NUM_HEADS', 8);
define('MODEL_FF_DIM', 512);
define('MODEL_NUM_LAYERS', 4);
define('CONTEXT_LENGTH', 256);

// Training Configuration
define('TRAIN_BATCH_SIZE', 32);
define('TRAIN_LEARNING_RATE', 0.001);
define('TRAIN_EPOCHS', 10);
define('TRAIN_VALIDATE_EVERY', 100);

// Generation Configuration
define('GENERATE_TEMPERATURE', 0.7);
define('GENERATE_TOP_K', 50);
define('GENERATE_TOP_P', 0.9);
define('GENERATE_MAX_TOKENS', 100);

// File Paths
define('MODEL_SAVE_PATH', __DIR__ . '/../models/');
define('DATA_PATH', __DIR__ . '/../data/');
define('CORPORA_PATH', __DIR__ . '/../data/corpora/');
define('VOCAB_SAVE_PATH', __DIR__ . '/../data/vocab/');

// API Configuration
define('API_DEBUG', true);
define('API_TIMEOUT', 30);