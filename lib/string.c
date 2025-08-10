#include "string.h"

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
