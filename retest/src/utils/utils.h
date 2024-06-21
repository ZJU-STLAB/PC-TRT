#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define INT_MAX 2147483647
#define INT_MIN -2147483648

typedef unsigned int uint32_t;

// Function to get length of a string
int get_length(const char *str) {
    int length = 0;
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

// Function to count the numbers in a string
int count_numbers(const char *str) {
    int len = get_length(str);
    assert(len >= 2);
    if(str[0] == '[' && str[1] == ']') {
        return 0;
    }
    int count = 0;
    for(int i = 0; i < len; i++) {
        if(str[i] == ',') {
            count++;
        }
    }
    return count + 1;
}

// Function to parse a string and return an array of integers
int* parse_string_to_array(const char *str) {
    int count = count_numbers(str);
    int *array = (int *)malloc(count * sizeof(int));
    int number = 0;
    int index = 0;
    int isNegative = 0;
    int stop = 0;
    int len = get_length(str);
    for (int i = 0; i <= len; i++) {
        if (str[i] >= '0' && str[i] <= '9' && !stop) {
            if(isNegative){
                // 如果越界了，返回INT_MIN
                if (number < INT_MIN / 10 || (number == INT_MIN / 10 && str[i] - '0' > 8)) {
                    number = INT_MIN;
                    stop = 1;
                }else{
                    number = number * 10 - (str[i] - '0');
                }
            }else{
                // 如果越界了，返回INT_MAX
                if (number > INT_MAX / 10 || (number == INT_MAX / 10 && str[i] - '0' > 7)) {
                    number = INT_MAX;
                    stop = 1;
                }else{
                    number = number * 10 + (str[i] - '0');
                }
            }
        } else if (str[i] == '-') {
            isNegative = 1;
        } else if (str[i] == ',' || str[i] == ']') {
            array[index++] = number;
            number = 0;
            isNegative = 0;
            stop = 0;
        }
    }
    return array;
}

uint32_t atou(const char *str) {
    uint32_t number = 0;
    int len = get_length(str);
    for (int i = 0; i < len; i++) {
        if(str[i] >= '0' && str[i] <= '9'){
            if(number > UINT32_MAX / 10 || (number == UINT32_MAX / 10 && str[i] - '0' > 5)){
                number = UINT32_MAX;
                break;
            }
            number = number * 10 + (str[i] - '0');
        }else{
            break;
        }
    }
    return number;
}

char* copy(const char *str) {
    int len = get_length(str);
    char *ret = (char*)malloc((len + 1) * sizeof(char));
    for (int i = 0; i < len; i++) {
        ret[i] = str[i];
    }
    ret[len] = '\0';
    return ret;
}