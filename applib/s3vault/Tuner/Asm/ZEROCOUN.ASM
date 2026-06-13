	mov	si,offset ftab
	callr	word ptr [bx+si]
	retf

ftab	dw	offset init

init	mov	si,ax
	mov	ax,0
	mov	dx,0[si]
	cmp	dx,0
	jg	first_is_neg
first_is_pos	mov	bx,1
	jmp	loop1
first_is_neg	mov	bx,0
loop1	mov	dx,0[si]
	cmp	dx,0
	jg	is_neg
is_pos	cmp	bx,0
	jne	no_inc
	inc	ax
	mov	bx,1
	jmp	no_inc
is_neg	cmp	bx,1
	jne	no_inc
	inc	ax
	mov	bx,0
no_inc	inc	si
	inc	si
	loop	loop1
	ret

