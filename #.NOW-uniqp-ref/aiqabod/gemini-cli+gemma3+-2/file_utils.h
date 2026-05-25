#ifndef FILE_UTILS_H
#define FILE_UTILS_H

// Function to read a file into a string buffer
char* read_file_to_string(const char* filename);

// Function to write content to a file
int write_string_to_file(const char* filename, const char* content, int auto_confirm);

#endif // FILE_UTILS_H
