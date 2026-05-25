#!/bin/bash

# Start PHP server for JavaScript LLM Terminal
echo "🚀 Starting PHP server for JavaScript LLM Terminal..."

# Check if PHP is installed (try both 'php' and '/usr/bin/php')
if ! command -v php &> /dev/null && ! [ -x "/usr/bin/php" ]; then
    echo "❌ PHP is not installed or not found in PATH. Please install PHP to run the backend."
    echo "💡 Try installing: sudo apt-get install php"
    exit 1
fi

# Use the available PHP command
if command -v php &> /dev/null; then
    PHP_CMD="php"
elif [ -x "/usr/bin/php" ]; then
    PHP_CMD="/usr/bin/php"
else
    echo "❌ Could not find PHP executable"
    exit 1
fi

# Create necessary directories if they don't exist
mkdir -p data/corpora
mkdir -p data/vocab
mkdir -p models
mkdir -p logs

echo "📁 Created required directories: data/corpora, data/vocab, models, logs"

# Check if port is available and find an available one
PORT=8080
while lsof -Pi :$PORT -sTCP:LISTEN -t >/dev/null 2>&1; do
    PORT=$((PORT + 1))
    if [ $PORT -gt 8090 ]; then
        echo "❌ Could not find available port between 8080-8090. Please free up a port."
        exit 1
    fi
done

echo "🌐 Starting PHP server on http://localhost:$PORT"
echo "💡 Access the interface at: http://localhost:$PORT/index.html"
echo "🔄 Press Ctrl+C to stop the server"

# Start the PHP built-in server using the router if it exists, otherwise without
if [ -f "router.php" ]; then
    $PHP_CMD -S localhost:$PORT router.php
else
    $PHP_CMD -S localhost:$PORT
fi