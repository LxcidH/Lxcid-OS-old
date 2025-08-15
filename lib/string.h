#ifndef STRING_H
#define STRING_H

#include <stddef.h>
int to_upper(int c);
int to_lower(int c);
void str_upper(char *str);
void str_lower(char *str);
int hex_to_int(const char* hex_str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
/**
 * @brief Calculates the length of a string.
 * @param str The null-terminated string to measure.
 * @return The number of characters in the string, excluding the null terminator.
 */
int strlen(const char* str);
/**
 * @brief Copies a string from a source to a destination.
 * @warning This function does not perform bounds checking. The destination
 * buffer must be large enough to hold the source string.
 * @param dest The destination buffer.
 * @param src The null-terminated source string to copy.
 * @return A pointer to the destination string.
 */
char* strcpy(char* dest, const char* src);

char* strcat(char* dest, const char* src);

char* itoa(int num, char* buffer, int base);

void* memcpy(void* dest, const void* src, size_t n);

void* memset(void* s, int c, size_t n);

#endif
