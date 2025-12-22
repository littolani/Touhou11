.686
.model flat

; Declare symbols as public so the linker can see them
PUBLIC _g_renderQuad144
PUBLIC _g_supervisor
PUBLIC _g_anmManager

_g_anmManager    EQU 04c3268h
_g_supervisor    EQU 04c3280h
_g_renderQuad144 EQU 04c91d8h

END