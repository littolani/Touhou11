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
PUBLIC _g_fpsCounter
PUBLIC _g_spellcard
PUBLIC _g_squareVertices
PUBLIC _g_gameSpeed
PUBLIC _g_globals
PUBLIC _g_player
PUBLIC _g_soundManager
PUBLIC _g_soundConfigTable
PUBLIC _g_anmRngContext
PUBLIC _g_replayRngContext
PUBLIC _g_app

; Functions
PUBLIC _game_free
PUBLIC _game_malloc
PUBLIC _game_new

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
_g_fpsCounter          EQU 04a8d80h
_g_spellcard           EQU 04a8d6ch
_g_squareVertices      EQU 04c9248h
_g_gameSpeed           EQU 04a7948h
_g_globals             EQU 04a56e0h
_g_player              EQU 04a8eb4h
_g_soundManager        EQU 04c3e80h
_g_soundConfigTable    EQU 04a34f0h
_g_anmRngContext       EQU 04c2ef8h
_g_replayRngContext    EQU 04c2f00h
_g_app                 EQU 04c9898h
_g_gdiObject_0         EQU 04c2eech;
_g_gdiObject_1         EQU 04c0ee4h;
_g_gdiObject_2         EQU 04c2ee8h;
_g_gdiObject_3         EQU 04a8ed4h;

.code
_game_free PROC
    mov eax, 045fd49h
    jmp eax
_game_free ENDP

_game_malloc PROC
    mov eax, 0460192h ; Game's malloc function
    jmp eax
_game_malloc ENDP

_game_new PROC
    mov eax, 045FCE4h  ; Address of the game's operator new
    jmp eax
_game_new ENDP

END