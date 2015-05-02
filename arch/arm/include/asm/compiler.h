#ifndef __ASM_ARM_COMPILER_H
#define __ASM_ARM_COMPILER_H

/*
 * This is used to ensure the compiler did actually allocate the register we
 * asked it for some inline assembly sequences.  Apparently we can't trust
 * the compiler from one version to another so a bit of paranoia won't hurt.
 * This string is meant to be concatenated with the inline asm string and
 * will cause compilation to stop on mismatch.
 * (for details, see gcc PR 15089)
 */
// 2015-05-02
// .ifnc
// Assembles if the strings are not the same.
// .ifnc string1, string2
// .ifnc "this", "that"
//
// .endif
// Ends an .if block.
//
// .err
// Generate an error durring assembly.
#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"


#endif /* __ASM_ARM_COMPILER_H */
