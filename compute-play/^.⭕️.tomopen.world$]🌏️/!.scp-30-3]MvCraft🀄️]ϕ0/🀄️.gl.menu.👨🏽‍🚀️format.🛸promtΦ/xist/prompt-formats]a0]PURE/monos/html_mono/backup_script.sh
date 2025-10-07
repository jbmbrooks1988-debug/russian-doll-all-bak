#!/bin/bash
# Backup script for CHTML Framework
# Creates a compressed archive of all project files

# Get the current directory
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Create backup directory if it doesn't exist
mkdir -p "$PROJECT_DIR/backup"

# Create a timestamped backup
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
BACKUP_FILE="$PROJECT_DIR/backup/chtml_framework_$TIMESTAMP.tar.gz"

# Create the archive with all project files
tar -czf "$BACKUP_FILE" -C "$PROJECT_DIR" *.c *.html *.sh *.md Makefile

echo "Backup created: $BACKUP_FILE"
echo "Backup size: $(du -h "$BACKUP_FILE" | cut -f1)"