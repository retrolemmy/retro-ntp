; start.s -- minimal startup for NTPSet (no C library). Sets SysBase, calls ntpmain(),
; returns its result in d0 as the CLI return code. Workbench start is handled in C.
	section	code,code

	xref	_ntpmain
	xref	_SysBase

start:
	move.l	4.w,a6
	move.l	a6,_SysBase
	jmp	_ntpmain

	dc.b	"$VER: NTPSet 1.0 (25.11.2026)",0
	even
