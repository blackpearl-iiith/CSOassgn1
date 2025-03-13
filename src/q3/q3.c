#include <stdio.h>

int palindrome(char *s, long size);
// {
//     for (long left = 0, right = size - 1; left < right; left++, right--) {
//         if (s[left] != s[right]) {
//             return 0;
//         }
//     }

//     return 1;
// }

int main() {
    char s[200001];
    
    scanf("%s", s);

    long size = 0;
    while (s[size] != '\0') {
        size++;
    }

    printf("%d\n", palindrome(s, size));

    return 0;
}
