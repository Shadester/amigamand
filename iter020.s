; The escape-time loop of the fixed-point build, everything in registers (vbcc spills most of it to the stack).
;
;   LONG iter_loop(__reg("a0") LONG *a)    a = { zr, zi, cr, ci, maxit, check, hit }, z and c in Q4.28
;
; Returns the iteration count (maxit = inside the set). The loop is the same as the C one (C_LOOP in view.c):
;   squares in Q8.24 are the high long of muls.l; while (i < maxit && zr2 + zi2 <= 4) {
;     zi = (hi(zr*zi) << 5) + ci;  zr = ((zr2 - zi2) << 4) + cr;  zr2 = hi(zr*zr);  zi2 = hi(zi*zi);  i++ }
;
; With check set, an orbit that comes back within EPS of a saved state (saved at iterations 1, 2, 4, 8, ...) is taken
; to be periodic and counted as inside (Brent), and hit is set to the iteration it was seen at. That is a guess, not a proof: fixed point never
; repeats a state exactly, so only a tolerance can find the cycle. It pays on views with low-period components and
; costs about a fifth of the speed where there are none, so the caller switches it off when it finds nothing.
;
; d2 zr, d3 zi, d4 cr, d5 ci, d6 zr2, a2 zi2, d7 i, a3 maxit, a1 4.0
; checked loop only: a4, a5 saved zr, zi minus EPS; a0 steps since the save, a6 steps to the next save

	machine	68020

EPS	equ	256		; 2^-20 in Q4.28

UPDATE	macro
	move.l	d2,d0
	muls.l	d3,d1:d0
	lsl.l	#5,d1
	add.l	d5,d1
	move.l	d1,d3			; zi = 2*zr*zi + ci
	move.l	d6,d0
	sub.l	a2,d0
	lsl.l	#4,d0
	add.l	d4,d0
	move.l	d0,d2			; zr = zr2 - zi2 + cr
	move.l	d2,d0
	muls.l	d2,d1:d0
	move.l	d1,d6			; zr2
	move.l	d3,d0
	muls.l	d3,d1:d0
	move.l	d1,a2			; zi2
	addq.l	#1,d7
	endm

SAVE	macro
	move.l	d2,d0
	sub.l	#EPS,d0
	move.l	d0,a4
	move.l	d3,d0
	sub.l	#EPS,d0
	move.l	d0,a5
	endm

	section	code,code

	public	_iter_loop
_iter_loop:
	move.l	a0,-(sp)
	movem.l	d2-d7/a2-a6,-(sp)
	movem.l	(a0),d2-d5
	move.l	16(a0),a3
	move.l	20(a0),a6		; check, until the loop starts
	move.l	#$04000000,a1
	moveq	#0,d7
	move.l	d2,d0
	muls.l	d2,d1:d0
	move.l	d1,d6
	move.l	d3,d0
	muls.l	d3,d1:d0
	move.l	d1,a2
	cmp.l	a3,d7
	bge	.done
	move.l	d6,d0
	add.l	a2,d0
	cmp.l	a1,d0
	bgt	.done
	move.l	a6,d0
	bne.s	.checked

.plain:
	UPDATE
	cmp.l	a3,d7
	bge	.done
	move.l	d6,d0
	add.l	a2,d0
	cmp.l	a1,d0
	ble.s	.plain
	bra	.done

.checked:
	SAVE
	suba.l	a0,a0
	moveq	#1,d0
	move.l	d0,a6
.cloop:
	UPDATE
	move.l	d2,d0
	sub.l	a4,d0
	cmp.l	#2*EPS,d0
	bcc.s	.nochk
	move.l	d3,d0
	sub.l	a5,d0
	cmp.l	#2*EPS,d0
	bcs.s	.inside
.nochk:
	addq.l	#1,a0
	cmpa.l	a6,a0
	bne.s	.cnext
	SAVE
	adda.l	a6,a6
	suba.l	a0,a0
.cnext:
	cmp.l	a3,d7
	bge	.done
	move.l	d6,d0
	add.l	a2,d0
	cmp.l	a1,d0
	ble.s	.cloop

.done:
	move.l	d7,d0
	movem.l	(sp)+,d2-d7/a2-a6
	addq.l	#4,sp
	rts

.inside:
	move.l	44(sp),a0		; the argument block
	move.l	d7,24(a0)		; hit: the iteration where the cycle was seen
	move.l	a3,d7
	bra	.done
