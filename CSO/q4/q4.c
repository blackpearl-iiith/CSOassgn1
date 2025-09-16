#include <stdio.h>

long difference(long *array, long size);
// {
//     long min = array[0];
//     long max = array[0];
//     for (long i = 1; i < size; i++)
//     {
//         if (array[i] < min)
//         {
//             min = array[i];
//         }
//         if (array[i] > max)
//         {
//             max = array[i];
//         }
//     }
//     return max - min;
// }

int main()
{
    long n;
    scanf("%ld", &n);

    long array[n];
    for (long i = 0; i < n; i++)
    {
        scanf("%ld", &array[i]);
    }

    long diff = difference(array, n);
    printf("%ld\n", diff);

    return 0;
}
