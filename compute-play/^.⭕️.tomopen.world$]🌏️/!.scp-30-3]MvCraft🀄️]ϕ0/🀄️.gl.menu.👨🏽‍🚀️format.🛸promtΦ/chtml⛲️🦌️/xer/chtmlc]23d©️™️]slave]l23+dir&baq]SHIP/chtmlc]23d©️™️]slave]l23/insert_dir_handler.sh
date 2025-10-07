#!/bin/bash

# Make a backup first
cp 4.controller.c 4.controller.c.backup2

# Use a different approach: extract the parts before and after the insertion point
# and insert our new code in between
{
    head -n 202 4.controller.c
    echo "                    } else if (strcmp(elements[i].type, \"dirlist\") == 0) {"
    echo "                        // Handle clicks on directory entries"
    echo "                        // Calculate which entry was clicked based on mouse position"
    echo "                        int line_height = 20;"
    echo "                        int header_height = 25; // Account for the \"dir element list:\" header"
    echo "                        int relative_y = ry - (parent_y + elements[i].y + header_height);"
    echo "                        int entry_index = relative_y / line_height;"
    echo ""
    echo "                        if (entry_index >= 0 && entry_index < elements[i].dir_entry_count) {"
    echo "                            // Check if the clicked entry is a directory (ends with \"/\")"
    echo "                            const char* clicked_entry = elements[i].dir_entries[entry_index];"
    echo "                            int entry_len = strlen(clicked_entry);"
    echo "                            if (entry_len > 0 && clicked_entry[entry_len-1] == '/') {"
    echo "                                // This is a directory - remove the \"/\" to get the actual directory name"
    echo "                                char dir_name[256];"
    echo "                                strncpy(dir_name, clicked_entry, entry_len-1);"
    echo "                                dir_name[entry_len-1] = '\\0';"
    echo ""
    echo "                                // Construct the new path"
    echo "                                char new_path[1024];"
    echo "                                if (strcmp(elements[i].dir_path, \".\") == 0 || strlen(elements[i].dir_path) == 0) {"
    echo "                                    strcpy(new_path, dir_name);"
    echo "                                } else {"
    echo "                                    snprintf(new_path, sizeof(new_path), \"%s/%s\", elements[i].dir_path, dir_name);"
    echo "                                }"
    echo ""
    echo "                                // Update the element's directory path and reload contents"
    echo "                                strncpy(elements[i].dir_path, new_path, 511);"
    echo "                                elements[i].dir_path[511] = '\\0';"
    echo "                                elements[i].dir_entry_count = 0; // Force reload on next render"
    echo ""
    echo "                                printf(\"Navigating to directory: %s (full path: %s)\\n\", dir_name, new_path);"
    echo ""
    echo "                                // Also handle directory selection event if there is an onClick handler"
    echo "                                if (strlen(elements[i].onClick) > 0) {"
    echo "                                    handle_element_event(elements[i].onClick);"
    echo "                                }"
    echo "                            } else {"
    echo "                                // This is a file - could implement file selection here in the future"
    echo "                                printf(\"File selected: %s\\n\", clicked_entry);"
    echo "                            }"
    echo "                        }"
    tail -n +203 4.controller.c
} > 4.controller.c.new && mv 4.controller.c.new 4.controller.c