#!/bin/bash

# Kill process running on port 8080
echo "🔍 Finding process running on port 8080..."
PID=$(lsof -t -i:8080)

if [ -z "$PID" ]; then
    echo "❌ No process found on port 8080"
else
    echo "mPid $PID found on port 8080"
    echo "💀 Killing process $PID..."
    kill -9 $PID
    sleep 1
    echo "✅ Process killed"
fi

# Verify the port is free
sleep 2
NEW_PID=$(lsof -t -i:8080)
if [ -z "$NEW_PID" ]; then
    echo "✅ Port 8080 is now free"
else
    echo "❌ Port 8080 is still in use by PID $NEW_PID"
fi