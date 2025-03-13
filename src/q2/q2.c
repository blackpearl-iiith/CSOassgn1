#include <stdio.h>

void rotateR(long *arr, long n);
// {
//     if (n <= 2) {
//         return; 
//     }
//     long second_last = arr[n - 2];
//     long last = arr[n - 1];

//     for (long i = n - 3; i >= 0; i--) {
//         arr[i + 2] = arr[i];
//     }

//     arr[0] = second_last;
//     arr[1] = last;
// }

int main() {
    long n;
    
    scanf("%ld", &n);

    long arr[n];
    for (long i = 0; i < n; i++) {
        scanf("%ld", &arr[i]);
    }

    rotateR(arr, n);
    
    for (long i = 0; i < n; i++) {
        printf("%ld ", arr[i]);
    }
    printf("\n");

    return 0;
}
