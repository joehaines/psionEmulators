	mov	si,offset ftab
	callr	word ptr [bx+si]
	retf

ftab	dw	offset init

init	mov	si,ax
	mov	ax,0
loop1	mov	dx,0[si]
	cmp	dx,ax
	jg	no_new_max
	mov 	ax,dx
no_new_max	inc	si
	inc	si
	loop	loop1
	ret

