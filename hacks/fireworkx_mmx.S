/*
 * Fast MMX blur code for Fireworkx
 * Copyright (c) 1999-2005 Rony B Chandran <ronybc@asia.com>
 *
 * From Kerala, INDIA
 * 
 * url: http://www.ronybc.8k.com
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 */
 
.data

.text

.align 8	

.global mmx_blur
.global mmx_glow

mmx_blur:      /* void mmx_blur( int *palaka,
                                 int width,
                                 int height)
                                 int fade_lvl) */
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	
	movl 32(%esp),%eax
	imull $0x00010001,%eax
	movd %eax,%mm0
	movq %mm0,%mm1
	psllq $32,%mm0
	por %mm0,%mm1
	movq %mm1,40(%esp)
	
	movl 20(%esp), %eax
	movl 24(%esp), %edx
	shll $2, %edx
	addl %eax, %edx
	pxor %mm0,%mm0
.fline:
	movq %mm0,(%eax)
	addl $8,%eax
	cmp %edx,%eax
	jbe .fline
	
	movl 24(%esp),%eax
	shll $2,%eax
	movl %eax,%edi
	movl 28(%esp),%edi
	subl $2,%edi
	imull %eax,%edi
	
	movl 20(%esp),%ecx
	movl %ecx,%ebx
	addl %eax,%ebx
	addl %ebx,%eax
	xorl %esi,%esi
.align 32
.renuKa:
	pxor %mm7,%mm7
	movq %mm0,%mm6
	movq %mm1,%mm0
	paddw %mm1,%mm6
	movd 8(%esi,%ecx),%mm1
	punpcklbw %mm7,%mm1
	paddw %mm1,%mm6

	paddw %mm2,%mm6
	movq %mm3,%mm2
	movd 8(%esi,%ebx),%mm3
	punpcklbw %mm7,%mm3
	paddw %mm3,%mm6

	paddw %mm4,%mm6
	movq %mm5,%mm4
	paddw %mm5,%mm6
	movd 8(%esi,%eax),%mm5
	punpcklbw %mm7,%mm5
	paddw %mm5,%mm6
	
	psllw $3,%mm2
	paddw %mm2,%mm6
	psrlw $3,%mm2
	movq 40(%esp),%mm7
	psubusw %mm7,%mm6 
	psrlw $4,%mm6
	
	packuswb %mm6,%mm6
	movd %mm6, 4(%esi,%ebx)
	addl $4,%esi
	cmp %edi,%esi
	jbe .renuKa
	
	addl %esi,%eax
	addl %esi,%ebx
	xorl %ecx,%ecx
.lline:
	movl %ecx,(%ebx)
	addl $4,%ebx
	cmpl %eax,%ebx
	jbe .lline
	
	emms
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	ret

/*  Add Glycerine to 
    Potassium permanganite... (DANGER-FIRE) */

mmx_glow:      /* void mmx_glow( int *palaka1,
                                 int width,
                                 int height,
                                 int fade_lvl,
                                 int *palaka2)  */
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	
	movl 32(%esp),%eax
	imull $0x00010001,%eax
	movd %eax,%mm0
	movq %mm0,%mm1
	psllq $32,%mm0
	por %mm0,%mm1
	movq %mm1,40(%esp)
	
	movl 20(%esp), %eax
	movl 36(%esp), %ebx
	movl 24(%esp), %edx
	shll $2,%edx
	add %eax, %edx
	pxor %mm7,%mm7
.flineGlow:
	movq %mm7,(%eax)
	movq %mm7,(%ebx)
	addl $8,%eax
	addl $8,%ebx
	cmp %edx,%eax
	jbe .flineGlow

	movl 24(%esp),%eax
	shll $2,%eax
	movl %eax,%edi
	movl 28(%esp),%edi
	subl $2,%edi
	imull %eax,%edi
	
	movl 20(%esp),%ecx
	movl 36(%esp),%edx
	movl %ecx,%ebx
	addl %eax,%edx
	addl %eax,%ebx
	addl %ebx,%eax
	xorl %esi,%esi
.align 32
.renuGa:
	pxor %mm7,%mm7
	movq %mm0,%mm6
	movq %mm1,%mm0
	paddw %mm1,%mm6
	movd 8(%esi,%ecx),%mm1
	punpcklbw %mm7,%mm1
	paddw %mm1,%mm6

	paddw %mm2,%mm6
	movq %mm3,%mm2
	movd 8(%esi,%ebx),%mm3
	punpcklbw %mm7,%mm3
	paddw %mm3,%mm6

	paddw %mm4,%mm6
	movq %mm5,%mm4
	paddw %mm5,%mm6
	movd 8(%esi,%eax),%mm5
	punpcklbw %mm7,%mm5
	paddw %mm5,%mm6
	
	psllw $3,%mm2
	paddw %mm2,%mm6
	psrlw $3,%mm2
	movq 40(%esp),%mm7
	psubusw %mm7,%mm6 
	movq %mm6,%mm7
	psrlw $4,%mm6
	psrlw $3,%mm7
	
	packuswb %mm7,%mm7
	movd %mm7, 4(%esi,%edx)
	packuswb %mm6,%mm6
	movd %mm6, 4(%esi,%ebx)
	addl $4,%esi
	cmp %edi,%esi
	jbe .renuGa

	addl %esi,%eax
	addl %esi,%ebx
	addl %esi,%edx
	xorl %ecx,%ecx
.llineGlow:
	movl %ecx,(%ebx)
	movl %ecx,(%edx)
	addl $4,%ebx
	addl $4,%edx
	cmpl %eax,%ebx
	jbe .llineGlow

	emms
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	ret
