#include "revert_string.h"

void RevertString(char *str)
{
    int left = 0;
    int right = 0;
    char temp;

    while (str[right] != '\0')
    {
        right++;
    }

    right--;

    while (left < right)
    {
        temp = str[left];
        str[left] = str[right];
        str[right] = temp;

        left++;
        right--;
    }
}

