#ifndef TYPECHECK_H_INCLUDED
#define TYPECHECK_H_INCLUDED

/*
 * Check at compile time that something is of a particular type.
 * Always evaluates to 1 so you may use it easily in comparisons.
 */

// Ref from ARM-10C.
// Compile Time에서 사용하는 기술이다.
// type이 같지 않으면 warnnig 발생.
// http://stackoverflow.com/questions/10393844/about-typecheck-in-linux-kernel
// http://www.iamroot.org/xe/Kernel_10_ARM/182249 - 10th 후기
// 예를 들어, 두개의 type이 다르다면, 비교구문에서 warning이 발생할 것이다.
// 그러므로, 개발자는 자신의 코드를 컴파일 타임에 검증할 수 있을 것이다.
/* 
 * http://tamtrajnana.blogspot.kr/2013/07/typechecking-with-gcc-typeof.html

There are intentions behind lines #3 and #4. The line 3 is just for generating warning of invalid comparison of types and not meant for any logical matching. The 'void' signifies to ignore the comparison value. The final '1' signifies always success. The value evaluated from compound statement is the final value evaluated in compound statement. If none is present, the evaluation is treated as void. The above macro is just for comparing types. This is reason why the evaluation of pointer comparison is masked off and final expression is *always* made to return true value.
 *
*/
#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})

/*
 * Check at compile time that 'function' is a certain type, or is a pointer
 * to that type (needs to use typedef for the function type.)
 */
#define typecheck_fn(type,function) \
({	typeof(type) __tmp = function; \
	(void)__tmp; \
})

#endif		/* TYPECHECK_H_INCLUDED */
