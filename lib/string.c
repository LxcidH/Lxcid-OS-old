#include "string.h"

int to_upper(int c) {
    // Check if character is in the lowercase range 'a' through 'z'
    if (c >= 'a' && c <= 'z') {
        // Convert to uppercase by subtracting the difference between 'a' and 'A'
        return c - 'a' + 'A';
    }

    // If not a lowercase letter, return it as is
    return c;
}

int to_lower(int c) {
    // Check if the character is in the uppercase range 'A' through 'Z'
    if (c >= 'A' && c <= 'Z') {
        // Convert to lowercase by adding the difference between 'a' and 'A'
        return c + ('a' - 'A');
    }

    // If not an uppercase letter, return it as is
    return c;
}

void str_upper(char *str) {
    // Return immediately if the pointer is null
    if (!str) {
        return;
    }

    // Loop until we hit the null terminator at the end of the string
    while (*str) {
        // Apply the toupper function to the current character
        // The cast to (unsigned char) is for safety with character values > 127
        *str = to_upper((unsigned char)*str);

        // Move the ptr to the next char
        str++;
    }
}

void str_lower(char *str) {
    // Return immediately if the pointer is null
    if (!str) {
        return;
    }

    // Loop until we hit the null terminator at the end of the string
    while (*str) {
        // Apply the to_lower function to the current character
        *str = to_lower(*str);

        // Move the pointer to the next character
        str++;
    }
}

int hex_to_int(const char* hex_str) {
    int result = 0;
    int i = 0;

    // Skip the 0x or 0X prefix if present
    if(hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        i = 2;
    }

    while(hex_str[i] != '\0') {
        char c = hex_str[i];
        int value = 0;

        if(c >= '0' && c <= '9') {
            value = c - '0';
        } else if(c >= 'a' && c <= 'f') {
            value = 10 + (c - 'a');
        } else if(c >= 'A' && c <= 'F') {
            value = 10 + (c - 'A');
        } else {
            // Invalid char stop parsing
            break;
        }
        result = (result * 16) + value;
        i++;
    }
    return result;
}

int strcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0) {
        // If the characters are different or we've hit a null terminator
        // in s1, break out of the loop
        if (*s1 != *s2 || *s1 == '\0') {
            break;
        }

        // Move to the next characters
        s1++;
        s2++;
        n--;
    }

    // If we've compared all n characters and found no differences
    // the strings are equal for that length
    if (n == 0) {
        return 0;
    }

    // Return the difference between the first non-matching characters.
    // Casting to uunsigned char is important for correct behaviour with characters outside the standard ASCII range
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strlen(const char* str) {
   size_t len = 0;
   while(str[len] != '\0') {
    len++;
   }
   return len;
}

char* strcpy(char* dest, const char* src) {
    // Save the original destination ptr to return later
    char* orig_dest = dest;

    // Loop until null term char is found in src string
    while(*src != '\0') {
        *dest = *src;   // Copy the character
        dest++;
        src++;
    }

    // After the loop, copy the null terminator to properly end the string
    *dest = '\0';

    // Return the original starting address of the destination string
    return orig_dest;
}

// Helper function to reverse a string in place
static void reverse(char* str, int length) {
    int start = 0;
    int end = length - 1;
    while(start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

char* strcat(char* dest, const char* src) {
    char* orig_dest = dest;

    // Find the end of the destination string
    while (*dest != '\0') {
        dest++;
    }

    // Copy the src str to the end of dest string
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }

    // Add the null terminator back
    *dest = '\0';

    return orig_dest;
}

/**
 * @brief Locates the last occurrence of a character in a string.
 *
 * @param s The string to be searched.
 * @param c The character to be located.
 * @return A pointer to the last occurrence of the character, or NULL if not found.
 */
char* strrchr(const char* s, int c) {
    const char* last_occurrence = NULL;

    // Iterate through the string until the null terminator is found.
    // The loop also checks the null terminator itself,
    // which is important because strrchr considers the null terminator as part of the string.
    do {
        // If the current character matches the target character,
        // update our pointer to the last found occurrence.
        if (*s == (char)c) {
            last_occurrence = s;
        }
    } while (*s++ != '\0'); // Increment s to move to the next character

    return (char*)last_occurrence;
}

/**
 * @brief Locates the first occurrence of a character in a string.
 *
 * @param str The string to be scanned.
 * @param c The character to be searched for.
 * @return A pointer to the first occurrence of the character c in the string str,
 * or NULL if the character is not found.
 */
char* strchr(const char* str, int c) {
    while (*str != '\0') {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    // Check for the null terminator as well, as per the C standard.
    if (c == '\0') {
        return (char*)str;
    }
    return NULL;
}

/**
 * @brief Converts an integer to a null-terminated string (itoa).
 * @param num The integer to convert.
 * @param buffer The buffer to store the resulting string.
 * @param base The numerical base to use (e.g., 10 for decimal, 16 for hex).
 * @return A pointer to the resulting string in the buffer.
 */
char* itoa(int num, char* buffer, int base) {
    int i = 0;
    int is_negative = 0;

    // Handle 0 explicitly
    if (num == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return buffer;
    }

    // Handle negative numbers only for base 10
    if(num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }

    // Process individual digits
    while(num != 0) {
        int rem = num % base;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    // If number is negative, append '-'
    if(is_negative) {
        buffer[i++] = '-';
    }

    buffer[i] = '\0'; // Null-terminate string

    // Reverse the string
    reverse(buffer, i);

    return buffer;
}

void* memcpy(void* dest, const void* src, size_t n) {
    // Cast the generic pointers to char pointers to work with bytes
    unsigned char* d = dest;
    const unsigned char* s = src;

    // Loop to copy n bytes
    while (n--) {
        *d++ = *s++;
    }

    // Return the original destination pointer
    return dest;
}

/**
 * Fills the first n bytes of the memory area pointed to by s
 * with the constant byte c.
 */
void* memset(void* s, int c, size_t n) {
    // Cast the generic pointer to a char pointer to work with bytes
    unsigned char* p = s;

    // Loop to set n bytes
    while (n--) {
        *p++ = (unsigned char)c;
    }

    // Return the original pointer
    return s;
}

/**
 * @brief Copies 'n' bytes from memory area 'src' to memory area 'dest'.
 * @details The memory areas may overlap; the copy is always done in a
 * non-destructive manner.
 * @param dest Pointer to the destination array.
 * @param src  Pointer to the source array.
 * @param n    The number of bytes to be copied.
 * @return A pointer to the destination, which is 'dest'.
 */
void* memmove(void* dest, const void* src, size_t n) {
    // Cast the void pointers to char pointers to work with bytes
    char* d = (char*)dest;
    const char* s = (const char*)src;

    // If the buffers don't overlap, or if the destination is before the source,
    // we can copy from start to finish (a forward copy).
    if (d <= s || d >= s + n) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    }
    // Otherwise, the destination overlaps and is after the source.
    // We must copy from end to start to avoid overwriting data we need to read.
    else {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

/**
 * @brief A pointer to save the position between subsequent calls to strtok.
 *
 * The 'static' keyword is crucial here. It means this variable retains its
 * value across multiple calls to the function. This is how strtok "remembers"
 * where it left off in the string.
 */
static char* next_token = NULL;

/**
 * @brief Parses a string into a sequence of tokens.
 *
 * On the first call, `strtok` should be called with the string to be parsed.
 * In subsequent calls, the first argument should be NULL.
 *
 * @param str The string to be tokenized, or NULL to continue from the last position.
 * @param delim A string containing the delimiter characters.
 * @return A pointer to the next token, or NULL if no more tokens are found.
 *
 * @warning This function is destructive! It writes null-terminators ('\0')
 * into the input string to separate tokens.
 */
char* strtok(char* str, const char* delim) {
    char* token_start;
    char* current_char;

    // --- 1. Determine where to start searching ---
    // If str is not NULL, this is a new tokenizing session.
    // Otherwise, continue from where we last left off.
    if (str != NULL) {
        next_token = str;
    }

    // If next_token is NULL, it means we've reached the end of the string.
    if (next_token == NULL || *next_token == '\0') {
        return NULL;
    }

    // --- 2. Skip leading delimiters ---
    // Move the start of our token past any initial delimiter characters.
    token_start = next_token;
    while (*token_start != '\0' && strchr(delim, *token_start) != NULL) {
        token_start++;
    }

    // If we've hit the end of the string after skipping delimiters, there are no more tokens.
    if (*token_start == '\0') {
        next_token = token_start; // Ensure next call returns NULL
        return NULL;
    }

    // --- 3. Find the end of the current token ---
    // Move forward until we find a delimiter or the end of the string.
    current_char = token_start;
    while (*current_char != '\0' && strchr(delim, *current_char) == NULL) {
        current_char++;
    }

    // --- 4. Terminate the token and save the next position ---
    // If we found a delimiter, replace it with a null terminator and
    // set `next_token` to the character immediately after it.
    if (*current_char != '\0') {
        *current_char = '\0';
        next_token = current_char + 1;
    } else {
        // We reached the end of the string, so there are no more tokens after this one.
        next_token = current_char;
    }

    return token_start;
}


