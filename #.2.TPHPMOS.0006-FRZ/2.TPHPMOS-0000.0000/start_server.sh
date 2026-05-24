#!/bin/sh
# TPMOS Starter Script (PHP/JS Version)

echo "Starting TPHPMOS (PHP/JS)..."

# Kill existing PHP server on this port if it exists
fuser -k 8000/tcp

# Start PHP server in the background
php -S localhost:8000 &

echo "Server running at http://localhost:8000"
echo "Press Ctrl+C to stop."
