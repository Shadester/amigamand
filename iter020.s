; The pixel loop of the fixed-point build, in registers (vbcc spills most of it to the stack, and a call for every pixel
; costs more than the few iterations most pixels need).
;
;   void run_loop(__reg("a0") LONG *a)       a = the block below, the pens of n pixels go to dst
;
;   0 cr (Q8.24, of the next pixel)   4 dx   8 ci   12 n   16 maxit   20 check   24 julia
;   28 jr (Q4.28)   32 ji   36 dst   40 lut   44 saved   48 spent   52 count   56 hit
;
; Each pixel is what view.c's C loop used to do (squares in Q8.24 are the high long of muls.l, z and c in Q4.28):
;   Mandelbrot: |c| > 2 is 1 iteration at once; inside the main cardioid or the period-2 bulb is maxit at once;
;   then  while (i < maxit && zr2 + zi2 <= 4) { zi = (hi(zr*zi) << 5) + ci;  zr = ((zr2 - zi2) << 4) + cr;
;                                               zr2 = hi(zr*zr);  zi2 = hi(zi*zi);  i++ }
;   Julia: z starts at the pixel, c is the constant.
; The pen is lut[i].
;
; With check set, an orbit that comes back within EPS of a saved state (saved at iterations 1, 2, 4, 8, ...) is taken
; to be periodic and counted as inside (Brent). That is a guess, not a proof: fixed point never repeats a state
; exactly, so only a tolerance can find the cycle. It pays on views with low-period components and costs about a
; fifth of the speed where there are none, so the caller switches it off when saved is small against spent.
; saved: iterations it spared, spent: iterations done, count: inside pixels, while it was on.
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

	public	_run_loop
_run_loop:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	a0,-(sp)
	tst.l	12(a0)
	ble	.exit

.pixel:
	move.l	(sp),a0
	move.l	(a0),d4			; the pixel, Q8.24
	move.l	8(a0),d5
	clr.l	56(a0)
	tst.l	24(a0)
	bne	.julia
	move.l	d4,d0
	add.l	#$02000000,d0
	cmp.l	#$04000000,d0
	bhi	.esc1			; |re| > 2: out after one iteration
	move.l	d5,d0
	add.l	#$02000000,d0
	cmp.l	#$04000000,d0
	bhi	.esc1			; |im| > 2
	cmp.l	#$FE800000,d4		; the cardioid and the bulb lie in -1.5 < re < 0.5, -1 < im < 1
	ble	.noc
	cmp.l	#$00800000,d4
	bge	.noc
	cmp.l	#$FF000000,d5
	ble	.noc
	cmp.l	#$01000000,d5
	bge	.noc
	lsl.l	#4,d4			; to Q4.28: this is c from here on
	lsl.l	#4,d5
	move.l	d5,d0
	muls.l	d5,d1:d0
	move.l	d1,d6			; yy = im^2
	move.l	d4,d0
	sub.l	#$04000000,d0		; xm = re - 1/4
	move.l	d0,a2
	muls.l	d0,d1:d0
	add.l	d6,d1			; q = xm^2 + yy
	move.l	d1,d0
	lsl.l	#4,d0
	move.l	d0,d1
	add.l	a2,d1			; q + xm
	muls.l	d1,d3:d0		; d3 = q * (q + xm)
	move.l	d6,d0
	asr.l	#2,d0
	cmp.l	d0,d3			; inside the cardioid: q * (q + xm) < yy / 4
	blt	.inmax
	move.l	d4,d0
	add.l	#$10000000,d0		; re + 1
	muls.l	d0,d1:d0
	add.l	d6,d1
	cmp.l	#$00100000,d1		; inside the bulb: (re + 1)^2 + yy < 1/16
	blt	.inmax
	moveq	#0,d2
	moveq	#0,d3
	bra.s	.core
.noc:
	lsl.l	#4,d4
	lsl.l	#4,d5
	moveq	#0,d2
	moveq	#0,d3
	bra.s	.core
.julia:
	move.l	d4,d2
	lsl.l	#4,d2			; z is the pixel
	move.l	d5,d3
	lsl.l	#4,d3
	move.l	28(a0),d4		; c is the constant
	move.l	32(a0),d5

.core:
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
	bge	.store
	move.l	d6,d0
	add.l	a2,d0
	cmp.l	a1,d0
	bgt	.store
	move.l	a6,d0
	bne.s	.checked

.plain:
	UPDATE
	cmp.l	a3,d7
	bge	.store
	move.l	d6,d0
	add.l	a2,d0
	cmp.l	a1,d0
	ble.s	.plain
	bra	.store

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
	bcs.s	.hit
.nochk:
	addq.l	#1,a0
	cmpa.l	a6,a0
	bne.s	.cnext
	SAVE
	adda.l	a6,a6
	suba.l	a0,a0
.cnext:
	cmp.l	a3,d7
	bge	.store
	move.l	d6,d0
	add.l	a2,d0
	cmp.l	a1,d0
	ble.s	.cloop
	bra	.store

.hit:
	move.l	(sp),a0
	move.l	d7,56(a0)		; the iteration where the cycle was seen
	move.l	a3,d7			; counted as inside
	bra	.store

.esc1:
	moveq	#1,d7
	bra.s	.store
.inmax:
	move.l	(sp),a0
	move.l	16(a0),d7

.store:
	move.l	(sp),a0
	tst.l	20(a0)
	beq.s	.nostat
	move.l	56(a0),d0
	beq.s	.nohit
	move.l	16(a0),d1
	sub.l	d0,d1
	add.l	d1,44(a0)		; saved += maxit - hit
	add.l	d0,48(a0)		; spent += hit
	addq.l	#1,52(a0)
	bra.s	.nostat
.nohit:
	add.l	d7,48(a0)
	cmp.l	16(a0),d7
	blt.s	.nostat
	addq.l	#1,52(a0)
.nostat:
	move.l	40(a0),a1
	move.b	(a1,d7.l),d0
	move.l	36(a0),a1
	move.b	d0,(a1)+
	move.l	a1,36(a0)
	move.l	4(a0),d0
	add.l	d0,(a0)			; the next pixel
	subq.l	#1,12(a0)
	bgt	.pixel
.exit:
	addq.l	#4,sp
	movem.l	(sp)+,d2-d7/a2-a6
	rts
