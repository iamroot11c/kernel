	.arch armv5t
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 34, 0
	.eabi_attribute 18, 4
	.file	"main.c"
	.section	.rodata
	.align	2
.LC0:
	.ascii	"<ECHO> %3d + %3d = %d\012\000"
	.text
	.align	2
	.global	echo
	.type	echo, %function
echo:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-8]
	str	r1, [fp, #-12]
	str	r2, [fp, #-16]
	ldr	r0, .L2
	ldr	r1, [fp, #-8]
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-16]
	bl	printf
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, pc}
.L3:
	.align	2
.L2:
	.word	.LC0
	.size	echo, .-echo
	.align	2
	.global	main
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	mov	r3, #3
	str	r3, [fp, #-16]
	mov	r3, #5
	str	r3, [fp, #-12]
	mov	r3, #0
	str	r3, [fp, #-8]
	mov	r0, #3
	mov	r1, #5
	bl	bit_set
	str	r0, [fp, #-8]
	mov	r3, #0
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, pc}
	.size	main, .-main
	.ident	"GCC: (Ubuntu/Linaro 4.7.3-12ubuntu1) 4.7.3"
	.section	.note.GNU-stack,"",%progbits
