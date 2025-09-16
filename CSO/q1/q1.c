#include <stdio.h>

long get(long arr[], long size);
// { 
//     long once = 0, twice = 0;

//     for (long i = 0; i < size; i++) {
//         once = (once ^ arr[i]) & ~twice;
//         twice = (twice ^ arr[i]) & ~once;
//     }

//     return once;
// }

int main() {
    long n;
    scanf("%ld", &n);

    long size = 3 * n + 1;
    long arr[size];
    for (long i = 0; i < size; i++) {
        scanf("%ld", &arr[i]);
    }

    long result = get(arr, size);
    printf("%ld\n", result);

    return 0;
}
