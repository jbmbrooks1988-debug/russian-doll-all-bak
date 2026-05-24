#!/bin/bash
# This script starts a PHP development server for the entire apps directory on localhost:8000
# and opens the browser to the MSR main menu

echo "Starting PHP development server for apps directory at http://localhost:8000"
cd /home/no/Desktop/qwen/dec-27/jobsh]b1/html.os=JBOSH]b0013/apps

# Start the PHP server in the background
php -S localhost:8000 &

# Wait a moment for the server to start
sleep 2

# Open the browser to the MSR index page
if command -v xdg-open &> /dev/null; then
    # Linux
    xdg-open http://localhost:8000/msr]PHP]dec27]c3/index.html
elif command -v open &> /dev/null; then
    # macOS
    open http://localhost:8000/msr]PHP]dec27]c3/index.html
elif command -v start &> /dev/null; then
    # Windows (Git Bash/Cygwin)
    start http://localhost:8000/msr]PHP]dec27]c3/index.html
else
    echo "Please open your browser and go to: http://localhost:8000/msr]PHP]dec27]c3/index.html"
fi

# Keep the script running to maintain the server
wait