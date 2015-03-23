/*
 * 원본 소스: arch/arm/lib/findbit.S
 */

.text

#define ALIGN   .align 4,0x90

#define STACK_PUSH \
  stmfd sp!, {fp, lr}; \
  add   fp, sp, #4; \
  sub   sp, sp, #16; 

#define ENTRY(name) \
  .globl name; \
  ALIGN; \
  name: \
  STACK_PUSH

#define STACK_POP \
  sub   sp, fp, #4; \
  ldmfd sp!, {fp, pc};

#define END(name) \
  .size name, .-name

#define ENDPROC(name) \
  STACK_POP \
  .type name, %function; \
  END(name) 

@-----------------------------------------------------------------------------@

.global echo;

@-----------------------------------------------------------------------------@

1:
    mov pc, lr

@-----------------------------------------------------------------------------@

ENTRY(bit_set)
         @b 1b  @ 위쪽에서 라벨을 찾음
         @b 1f  @ 아래쪽 라벨을 찾음
         add r1, #9
1:
         add r2, r0, r1
         bl echo
         mov r0, r2
         mov pc, lr
ENDPROC(bit_set)

/*
 * Purpose  : Find a 'zero' bit
 * Prototype: int find_first_zero_bit(void *addr, unsigned int maxbit);
 */
ENTRY(_find_first_zero_bit_le)
                teq     r1, #0
                beq     3f
                mov     r2, #0
1:
                ldrb    r3, [r0, r2, lsr #3]    
                eors    r3, r3, #0xff           @ invert bits
                bne     .L_found                @ any now set - found zero bit
                add     r2, r2, #8              @ next bit pointer
2:              cmp     r2, r1                  @ any more?
                blo     1b
3:              mov     r0, r1                  @ no free bits
                mov     pc, lr
ENDPROC(_find_first_zero_bit_le)

/*
 * Purpose  : Find next 'zero' bit
 * Prototype: int find_next_zero_bit(void *addr, unsigned int maxbit, int offset)
 */
/*
 * 참고: http://u-city.tistory.com/143
 *       http://pinocc.tistory.com/archive/201305
 *       모기향: 261 ~ 263쪽
 * bdata->node_bootmem_map, midx, sidx
 */
ENTRY(_find_next_zero_bit_le)
                teq     r1, #0
                beq     3b                      @// 레이블 3으로 이동하여 종료
                ands    ip, r2, #7              @ ip = r2 & #7
                beq     1b                      @ If new byte, goto old routine
                ldrb    r3, [r0, r2, lsr #3]    @// 주소가 8의 배수로 구성됨
                eor     r3, r3, #0xff           @ now looking for a 1 bit
                                                @// 1byte만 비트를 바꿈
                movs    r3, r3, lsr ip          @ shift off unused bits
                                                @// 최대 우측 쉬프트
                bne     .L_found
                orr     r2, r2, #7              @ if zero, then no bits here
                add     r2, r2, #1              @ align bit pointer
                b       2b                      @ loop for next bit
ENDPROC(_find_next_zero_bit_le)

/*
 * Purpose  : Find a 'one' bit
 * Prototype: int find_first_bit(const unsigned long *addr, unsigned int maxbit);
 */
// 2015-02-28;
ENTRY(_find_first_bit_le)
                teq     r1, #0
                beq     3f
                mov     r2, #0
1:
                ldrb    r3, [r0, r2, lsr #3]   
                movs    r3, r3
                bne     .L_found                @ any now set - found zero bit
                add     r2, r2, #8              @ next bit pointer
2:              cmp     r2, r1                  @ any more?
                blo     1b
3:              mov     r0, r1                  @ no free bits
                mov     pc, lr
ENDPROC(_find_first_bit_le)

/*
 * Purpose  : Find next 'one' bit
 * Prototype: int find_next_zero_bit(void *addr, unsigned int maxbit, int offset)
 */
// 2015-02-28;
//
// _find_next_bit_le(cpumask_bits(srcp)/*maskp->bits*/, nr_cpumask_bits/*2*/, n+1)
// r0 = maskp->bits
// r1 = 2
// r2 = n+1 // 0, 1, 2, 3, ... , n;
ENTRY(_find_next_bit_le)
                teq     r1, #0
                beq     3b              @// backward 3 label
                ands    ip, r2, #7      @// ip = r2 & 7;
                                        @// 오프셋이 0 또는 8(2^3),
                                        @// 16(2^4), 32(2^5), ... , 2^n 이면
                                        @// 값을 우측으로 쉬프트 하지 않음
                beq     1b                      @ If new byte, goto old routine
                ldrb    r3, [r0, r2, lsr #3] @// r3 = *(r0 + (r2 >> 3)) // *(r0 + (r2/8))
                movs    r3, r3, lsr ip          @ shift off unused bits
                                        @// r3 = r3 >> ip;
                                        @// 오프셋 & 7 연산 후 나머지 만큼
                                        @// 우측으로 쉬프트
                                        @// 이는 찾는 위치를 0번째 비트에서
                                        @// 하기위해 우측으로 쉬프트
                bne     .L_found        @// if (r3 > 0) .L_fouund;
                orr     r2, r2, #7      @ if zero, then no bits here
                add     r2, r2, #1      @ align bit pointer
                                        @// r2 = (r2|0b0111) + 1;
                                        @// 8비트(0~7, 8~15, 16~21, 22~32)단위로
                                        @// 검사를 먼저하여 다음 구역으로 이동
                                        @// 예를 들어 검사는 위치가 8번 비트에서
                                        @// 15번 비트라면
                                        @// 16번 비트에서 21번 비트사이를 검사하기
                                        @// 위해 오프셋을 조정
                b       2b              @ loop for next bit
                                        @// backward 2 label
ENDPROC(_find_next_bit_le)


#ifdef __ARMEB__

ENTRY(_find_first_zero_bit_be)
                teq     r1, #0
                beq     3f
                mov     r2, #0
1:              eor     r3, r2, #0x18           @ big endian byte ordering
                ldrb    r3, [r0, r3, lsr #3] 
                eors    r3, r3, #0xff           @ invert bits
                bne     .L_found                @ any now set - found zero bit
                add     r2, r2, #8              @ next bit pointer
2:              cmp     r2, r1                  @ any more?
                blo     1b
3:              mov     r0, r1                  @ no free bits
                mov     pc, lr
ENDPROC(_find_first_zero_bit_be)

ENTRY(_find_next_zero_bit_be)
                teq     r1, #0
                beq     3b
                ands    ip, r2, #7
                beq     1b                      @ If new byte, goto old routine
                eor     r3, r2, #0x18           @ big endian byte ordering
                ldrb    r3, [r0, r3, lsr #3]
                eor     r3, r3, #0xff           @ now looking for a 1 bit
                movs    r3, r3, lsr ip          @ shift off unused bits
                bne     .L_found
                orr     r2, r2, #7              @ if zero, then no bits here
                add     r2, r2, #1              @ align bit pointer
                b       2b                      @ loop for next bit
ENDPROC(_find_next_zero_bit_be)

ENTRY(_find_first_bit_be)
                teq     r1, #0
                beq     3f
                mov     r2, #0
1:              eor     r3, r2, #0x18           @ big endian byte ordering
                ldrb    r3, [r0, r3, lsr #3]
                movs    r3, r3
                bne     .L_found                @ any now set - found zero bit
                add     r2, r2, #8              @ next bit pointer
2:              cmp     r2, r1                  @ any more?
                blo     1b
3:              mov     r0, r1                  @ no free bits
                mov     pc, lr
ENDPROC(_find_first_bit_be)

ENTRY(_find_next_bit_be)
                teq     r1, #0
                beq     3b
                ands    ip, r2, #7
                beq     1b                      @ If new byte, goto old routine
                eor     r3, r2, #0x18           @ big endian byte ordering
                ldrb    r3, [r0, r3, lsr #3]
                movs    r3, r3, lsr ip          @ shift off unused bits
                bne     .L_found
                orr     r2, r2, #7              @ if zero, then no bits here
                add     r2, r2, #1              @ align bit pointer
                b       2b                      @ loop for next bit
ENDPROC(_find_next_bit_be)

#endif

ENTRY(_cnt_first_bit)
    clz r0, r0
ENDPROC(_cnt_first_bit)

/*
 * cmp test
 */
ENTRY(less_than)
    cmp r0, r1     @// if (r0 < r1)
    movlo r0, r1   @//   r0 = r1;
@    movlt r0, #13   @//   r0 = r1;
ENDPROC(_cnt_first_bit)

ENTRY(greater_or_equal)
    cmp r0, r1     @// if (r0 >= r1)
    movhs r0, #29   @//   r0 = r1;
ENDPROC(_cnt_first_bit)

/*
 * One or more bits in the LSB of r3 are assumed to be set.
 */
/*
 * 참고: http://forum.falinux.com/zbxe/index.php?document_srl=574399&mid=lecture_tip
 */
 .L_found:
         rsb     r0, r3, #0            @// r0 = #0 - r3
         and     r3, r3, r0            @// r3 = r3 & (-r3);
                                       @//  13 = 0b0000_1101
                                       @// -13 = 0b1111_0011
                                       @// 0b0000_0001 = 0b0000_1101 & 0b1111_0011;
         clz     r3, r3                @// 최상위 비트에서 최초로 1이 나올때까지의
                                       @// 비트개수로 위의 예제에서는 31이 나옴
                                       @//  13 = 0b0000_1101     44 = 0b0010_1100
                                       @// -13 = 0b1111_0011    -44 = 0b1101_0100
                                       @// -----------------    -----------------
                                       @//       0b0000_0001          0b0000_0100
                                       @// r3 = 32 - 31              r3 = 32 - 29
                                       @//    = 1                       = 3
         rsb     r3, r3, #31           @// r3 = #31 - r3; // 0 <= r3 < 31
                                       @// 31에서 찾은 위치를 빼면 맨 처음 셋되어있는
                                       @// 비트를 찾게됨
                                       @// fls(), ffs(), ffz() 함수 참고
         add     r0, r2, r3            @// r0 = r2 + r3


         cmp     r1, r0                @ Clamp to maxbit
         movlo   r0, r1                @// if (r1 < r0)
                                       @//  r0 = r1
                                       @// 조사 범위를 넘었는지 조사
 @        mov     pc, lr
         STACK_POP
         
         
         
