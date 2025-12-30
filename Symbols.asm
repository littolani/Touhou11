.686
.model flat

; Declare symbols as public so the linker can see them
PUBLIC _g_renderQuad144
PUBLIC _g_supervisor
PUBLIC _g_anmManager
PUBLIC _g_chain
PUBLIC _g_window
PUBLIC _g_pbgArchive
PUBLIC _g_pbgArchives
PUBLIC _g_numEntriesInDatFile
PUBLIC _g_lzssDict
PUBLIC _g_lzssTree

; Functions
PUBLIC _game_free
PUBLIC _game_malloc

_g_anmManager          EQU 04c3268h
_g_supervisor          EQU 04c3280h
_g_renderQuad144       EQU 04c91d8h
_g_chain               EQU 04c3234h
_g_window              EQU 04c3d88h
_g_pbgArchive          EQU 04c9680h
_g_pbgArchives         EQU 04c3c48h
_g_numEntriesInDatFile EQU 04a8ed0h
_g_lzssDict            EQU 04c0ee8h
_g_lzssTree            EQU 04a8ed8h


.code
_game_free PROC
    mov eax, 045fd49h
    jmp eax
_game_free ENDP

_game_malloc PROC
    mov eax, 0460192h ; Game's malloc function
    jmp eax
_game_malloc ENDP

END