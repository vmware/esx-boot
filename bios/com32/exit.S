/* -----------------------------------------------------------------------
 *
 *   Copyright 2003-2008 H. Peter Anvin - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * Implementation of _exit() for com32 based on c32entry.S
 */
	.text

	.globl _Exit
	.type _Exit, @function
_Exit:
	/* Just fall through to _exit */
	.size _Exit, .-_Exit

	.globl _exit
	.type _exit, @function
_exit:
#ifdef REGPARM
	pushl %eax
#endif

	/* Run any destructors */
	movl $__dtors_start, %esi
2:
	cmpl $__dtors_end, %esi
	jae 1f
	call *(%esi)
	addl $4,%esi
	jmp 2b

1:
#ifdef REGPARM
	popl %eax
#else
	movl 4(%esp),%eax		# Exit code in %eax = return value
#endif
	movl (__entry_esp),%esp		# Return stack pointer to entry value
	ret				# Return to termination address
	.size _exit, .-_exit

	.data
__exit_handler:
	.globl __exit_handler
	.long _exit
