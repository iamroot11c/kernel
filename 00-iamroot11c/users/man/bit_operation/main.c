#include <stdio.h>

int bit_set(int, int);
int _cnt_first_bit(int);
int less_than(int, int);
int greater_or_equal(int, int);

///////////////////////////////////////////////////////////////////////////////

/*
 * On ARMv5 and above those functions can be implemented around
 * the clz instruction for much better code efficiency.
 */

// 2015-01-17
// x : 4096 -> return 13;
// 처음으로 1이 나온 비트 number를 리턴
// 2015-02-28; 내용 보충
// fls (find last set)
// ffs (find first set)
// ffz (find first zero)
//
// 명명 규칙은 아래와 같다.
// MSB ............... LSB
// LSB부터 찾을 때 first
// MSB부터 찾을 때 last

// set - 1을 찾는다.
// zero - 0을 찾는다.
// 참고: http://u-city.tistory.com/143
//
// 셋되어있는 맨 마지막 비트의 위치를 찾음
static inline int fls(int x)
{
    int ret;

    if (__builtin_constant_p(x))
           return constant_fls(x);

    asm("clz\t%0, %1" : "=r" (ret) : "r" (x));
        ret = 32 - ret;
    return ret;
}

#define __fls(x) (fls(x) - 1)
//2015-02-28; 처음으로 셋되어 있는 비트를 찾음
#define ffs(x) ({ unsigned long __t = (x); fls(__t & -__t); })
#define __ffs(x) (ffs(x) - 1)
// find first zero
// 첫번째 1이 나오기까지 0의 개수를 구함
#define ffz(x) __ffs( ~(x) )

///////////////////////////////////////////////////////////////////////////////

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
