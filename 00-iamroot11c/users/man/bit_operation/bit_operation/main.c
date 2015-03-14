#include <stdio.h>

int bit_set(int, int);
int _cnt_first_bit(int);
int less_than(int, int);
int greater_or_equal(int, int);

int echo(int v1, int v2, int ret)
{
    printf("<ECHO> %3d + %3d = %d\n", v1, v2, ret);
}

int main()
{
    int v1 = 3, v2 = 5, ret = 0;

//    ret = bit_set(3, 5);
    //printf("%3d + %3d = %d\n", 3, 5, ret);

    ret = _cnt_first_bit(0x01000000);
    printf("cnt bits: %03d\n", ret);

    ret = less_than(34, 134);
    printf("less than: %04d\n", ret);

    ret = greater_or_equal(134, 134);
    printf("greater or equal: %04d\n", ret);

    ret = greater_or_equal(34, 86);
    printf("greater or equal: %04d\n", ret);

    return 0;
}
