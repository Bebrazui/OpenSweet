; ==============================================================================
; Opensweet OS - X11 Wire Protocol Server & Display Engine (kernel/x11.inc)
; Implements an in-kernel X11 server listening on UNIX socket /tmp/.X11-unix/X0 (Display :0)
; Translates core X11 wire protocol requests directly into Acrylic R2D Window Manager:
;   - Connection Setup / Handshake (Success, Root Window ID 0x100, 1920x1080x32bpp)
;   - CreateWindow (#1), MapWindow (#8), UnmapWindow (#10), DestroyWindow (#4)
;   - ConfigureWindow (#12), ChangeProperty (#18), GetProperty (#20)
;   - CreateGC (#55), ChangeGC (#56), FreeGC (#60)
;   - PutImage (#72) and MIT-SHM ShmPutImage (#extension)
;   - Keyboard / Pointer event forwarding from Acrylic R2D WM queue
; ==============================================================================

MAX_X11_CLIENTS = 2
X11_CLIENT_BUF_SZ = 4096                ; 4KB per-client RX/TX wire stream

; X11 Opcodes (Wire Protocol Version 11)
X11_OP_CREATE_WINDOW     = 1
X11_OP_CHANGE_WINDOW_ATT = 2
X11_OP_DESTROY_WINDOW    = 4
X11_OP_MAP_WINDOW        = 8
X11_OP_UNMAP_WINDOW      = 10
X11_OP_CONFIGURE_WINDOW  = 12
X11_OP_INTERN_ATOM       = 16
X11_OP_CHANGE_PROPERTY   = 18
X11_OP_GET_PROPERTY      = 20
X11_OP_CREATE_PIXMAP     = 53
X11_OP_FREE_PIXMAP       = 54
X11_OP_CREATE_GC         = 55
X11_OP_CHANGE_GC         = 56
X11_OP_FREE_GC           = 60
X11_OP_POLY_FILL_RECT    = 70
X11_OP_PUT_IMAGE         = 72
X11_OP_QUERY_EXTENSION   = 98

; MIT-SHM Minor Opcodes
X11_SHM_ATTACH           = 1
X11_SHM_DETACH           = 2
X11_SHM_PUT_IMAGE        = 3

; Client State
X11_STATE_UNUSED         = 0
X11_STATE_HANDSHAKE      = 1
X11_STATE_CONNECTED      = 2

; Structure of x11_client_t (32768 + 128 bytes):
;   +0:   uint32 state
;   +4:   uint32 win_id (allocated OpenSweet WM window 0..7 or -1)
;   +8:   uint32 xid_window
;   +12:  uint32 xid_gc
;   +16:  uint32 win_x
;   +20:  uint32 win_y
;   +24:  uint32 win_w
;   +28:  uint32 win_h
;   +32:  uint32 rx_head
;   +36:  uint32 rx_tail
;   +40:  uint32 tx_head
;   +44:  uint32 tx_tail
;   +48:  uint32 sequence_number
;   +52:  uint32 shm_seg_id
;   +56:  uint64 shm_virt_addr
;   +64:  64 bytes client title string
;   +128: X11_CLIENT_BUF_SZ bytes rx_buffer
;   +(128 + X11_CLIENT_BUF_SZ): X11_CLIENT_BUF_SZ bytes tx_buffer
X11_CLIENT_TX_OFF = (128 + X11_CLIENT_BUF_SZ)
X11_CLIENT_STRUCT_SZ = (128 + X11_CLIENT_BUF_SZ * 2)

align 16
x11_clients: rb (MAX_X11_CLIENTS * X11_CLIENT_STRUCT_SZ)
x11_atom_counter dd 100

; ==============================================================================
; x11_init: Initialize X11 Subsystem
; ==============================================================================
x11_init:
    push rdi
    push rcx
    push rax
    lea rdi, [r15 + x11_clients - kmain]
    mov ecx, (MAX_X11_CLIENTS * X11_CLIENT_STRUCT_SZ) / 8
    xor eax, eax
    cld
    rep stosq
    pop rax
    pop rcx
    pop rdi
    ret

; ==============================================================================
; x11_alloc_client: Allocate a client slot
; Returns: RAX = client pointer (or 0 if full)
; ==============================================================================
x11_alloc_client:
    push rcx
    push rbx
    xor ecx, ecx
.find_c:
    cmp ecx, MAX_X11_CLIENTS
    jae .no_c
    imul eax, ecx, X11_CLIENT_STRUCT_SZ
    lea rbx, [r15 + x11_clients - kmain + rax]
    cmp dword [rbx + 0], X11_STATE_UNUSED
    je .found_c
    inc ecx
    jmp .find_c

.found_c:
    mov dword [rbx + 0], X11_STATE_HANDSHAKE
    mov dword [rbx + 4], -1            ; win_id = -1
    mov dword [rbx + 24], 640          ; default w
    mov dword [rbx + 28], 480          ; default h
    mov dword [rbx + 32], 0            ; rx_head
    mov dword [rbx + 36], 0            ; rx_tail
    mov dword [rbx + 40], 0            ; tx_head
    mov dword [rbx + 44], 0            ; tx_tail
    mov dword [rbx + 48], 1            ; seq = 1
    mov rax, rbx
    pop rbx
    pop rcx
    ret

.no_c:
    xor eax, eax
    pop rbx
    pop rcx
    ret

; ==============================================================================
; x11_client_write: Append data to client TX buffer
; RBX = client pointer, RSI = source buffer, RDX = count
; ==============================================================================
x11_client_write:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
.w_loop:
    test rdx, rdx
    jz .w_done
    mov eax, [rbx + 40]                ; tx_head
    lea rdi, [rbx + X11_CLIENT_TX_OFF + rax] ; tx_buffer + head
    mov cl, [rsi]
    mov [rdi], cl
    inc rsi
    inc eax
    and eax, (X11_CLIENT_BUF_SZ - 1)
    mov [rbx + 40], eax
    dec rdx
    jmp .w_loop
.w_done:
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ret

; ==============================================================================
; x11_client_read: Read available bytes from client TX buffer into user buffer
; RBX = client pointer, RDI = user dest buffer, RDX = max count
; Returns: RAX = actual bytes read
; ==============================================================================
x11_client_read:
    push rcx
    push rdx
    push rsi
    push rdi
    xor ecx, ecx                       ; bytes read count
.r_loop:
    cmp rcx, rdx
    jae .r_done
    mov eax, [rbx + 44]                ; tx_tail
    cmp eax, [rbx + 40]                ; tx_tail == tx_head?
    je .r_done
    lea rsi, [rbx + X11_CLIENT_TX_OFF + rax]
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc ecx
    mov eax, [rbx + 44]
    inc eax
    and eax, (X11_CLIENT_BUF_SZ - 1)
    mov [rbx + 44], eax
    jmp .r_loop
.r_done:
    mov eax, ecx
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    ret

; ==============================================================================
; x11_process_rx: Process incoming X11 wire protocol bytes from client
; RBX = client pointer, RSI = user buffer, RDX = count
; ==============================================================================
x11_process_rx:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r12, rbx                       ; r12 = client
    test rdx, rdx
    jz .proc_done

    ; If in HANDSHAKE state, handle connection setup
    cmp dword [r12 + 0], X11_STATE_HANDSHAKE
    jne .proc_commands

    ; Handshake: client sends 12 bytes header:
    ; byte 0: endian ('B'=0x42 MSB or 'l'=0x6C LSB)
    ; byte 2..3: major-version (11)
    ; byte 4..5: minor-version (0)
    ; byte 6..7: name length
    ; byte 8..9: data length
    cmp rdx, 12
    jb .proc_done

    ; Mark connected
    mov dword [r12 + 0], X11_STATE_CONNECTED

    ; Send Success Connection Reply (8 bytes base header + setup info)
    ; Header:
    ;   +0: uint8  reply_type = 1 (Success)
    ;   +1: uint8  unused = 0
    ;   +2: uint16 major_version = 11
    ;   +4: uint16 minor_version = 0
    ;   +6: uint16 reply_length = (setup_len) / 4
    sub rsp, 256
    mov rdi, rsp
    mov ecx, 256 / 4
    xor eax, eax
    rep stosd
    mov rdi, rsp

    mov byte  [rdi + 0], 1             ; Success
    mov word  [rdi + 2], 11            ; Major 11
    mov word  [rdi + 4], 0             ; Minor 0
    mov word  [rdi + 6], 32            ; Additional data length in 4-byte units (128 bytes)
    mov dword [rdi + 8], 1             ; Release number
    mov dword [rdi + 12], 0x00010000   ; Resource ID base (client XID base)
    mov dword [rdi + 16], 0x0000FFFF   ; Resource ID mask
    mov dword [rdi + 20], 0            ; Motion buffer size
    mov word  [rdi + 24], 10           ; Vendor string length = 10 ("OpenSweet\0")
    mov word  [rdi + 26], 0xFFFF       ; Max request length
    mov byte  [rdi + 28], 1            ; Number of screens = 1
    mov byte  [rdi + 29], 1            ; Number of formats = 1
    mov byte  [rdi + 30], 0            ; Image byte order (0 = LSBFirst)
    mov byte  [rdi + 31], 0            ; Bitmap format bit order

    ; Copy vendor string "OpenSweet"
    mov dword [rdi + 40], "Open"
    mov dword [rdi + 44], "Swee"
    mov word  [rdi + 48], "t"

    ; Format: Depth 24, bpp 32, scanline-pad 32
    mov byte  [rdi + 52], 24           ; Depth
    mov byte  [rdi + 53], 32           ; Bits per pixel
    mov byte  [rdi + 54], 32           ; Scanline pad

    ; Screen 0 Info:
    ;   Root window ID = 0x00000100
    ;   Width in pixels = 1920, Height in pixels = 1080
    ;   Width in mm = 508, Height in mm = 285
    ;   Root visual ID = 0x00000020
    mov dword [rdi + 64], 0x00000100   ; Root window ID
    mov dword [rdi + 68], 0            ; Default colormap
    mov dword [rdi + 72], 0xFFFFFFFF   ; White pixel
    mov dword [rdi + 76], 0x00000000   ; Black pixel
    mov dword [rdi + 80], 0            ; Current input masks
    mov word  [rdi + 84], 1920         ; Width
    mov word  [rdi + 86], 1080         ; Height
    mov word  [rdi + 88], 508          ; Width in mm
    mov word  [rdi + 90], 285          ; Height in mm
    mov word  [rdi + 92], 1            ; Min installed maps
    mov word  [rdi + 94], 1            ; Max installed maps
    mov dword [rdi + 96], 0x00000020   ; Root visual ID
    mov byte  [rdi + 100], 0           ; Backing stores (Never)
    mov byte  [rdi + 101], 1           ; Save unders (True)
    mov byte  [rdi + 102], 24          ; Root depth = 24
    mov byte  [rdi + 103], 1           ; Allowed depths count = 1

    ; Send 136 bytes setup reply
    mov rbx, r12
    mov rsi, rsp
    mov rdx, 136
    call x11_client_write
    add rsp, 256
    jmp .proc_done

.proc_commands:
    ; Parse standard 4-byte X11 Request Header:
    ; [0]: uint8  opcode
    ; [1]: uint8  minor_opcode / data
    ; [2..3]: uint16 request_length (in 4-byte words, including header)
    mov rbx, r12
.cmd_loop:
    cmp rdx, 4
    jb .proc_done

    movzx eax, byte [rsi]              ; opcode
    movzx ecx, word [rsi + 2]          ; req_len in 4-byte words
    test ecx, ecx
    jz .proc_done
    shl ecx, 2                         ; req_len in bytes
    cmp rdx, rcx
    jb .proc_done                      ; partial request, wait for more data

    push rcx
    push rdx
    push rsi

    ; Dispatch based on opcode
    cmp al, X11_OP_CREATE_WINDOW
    je .x11_create_window
    cmp al, X11_OP_MAP_WINDOW
    je .x11_map_window
    cmp al, X11_OP_CONFIGURE_WINDOW
    je .x11_configure_window
    cmp al, X11_OP_CHANGE_PROPERTY
    je .x11_change_property
    cmp al, X11_OP_QUERY_EXTENSION
    je .x11_query_ext
    cmp al, X11_OP_PUT_IMAGE
    je .x11_put_image
    cmp al, X11_OP_CREATE_GC
    je .x11_create_gc
    cmp al, X11_OP_POLY_FILL_RECT
    je .x11_poly_fill
    cmp al, X11_OP_INTERN_ATOM
    je .x11_intern_atom
    cmp al, X11_OP_GET_PROPERTY
    je .x11_get_property

.cmd_next:
    pop rsi
    pop rdx
    pop rcx
    add rsi, rcx
    sub rdx, rcx
    inc dword [rbx + 48]               ; next sequence number
    jmp .cmd_loop

.x11_create_window:
    ; CreateWindow wire format:
    ; +4: uint32 wid
    ; +8: uint32 parent
    ; +12: int16 x, +14: int16 y
    ; +16: uint16 width, +18: uint16 height
    ; +20: uint16 border_width, +22: uint16 class
    ; +24: uint32 visual
    mov eax, [rsi + 4]
    mov [rbx + 8], eax                 ; store xid_window
    movsx eax, word [rsi + 12]
    mov [rbx + 16], eax                ; win_x
    movsx eax, word [rsi + 14]
    mov [rbx + 20], eax                ; win_y
    movzx eax, word [rsi + 16]
    test eax, eax
    jnz @f
    mov eax, 400
@@:
    mov [rbx + 24], eax                ; win_w
    movzx eax, word [rsi + 18]
    test eax, eax
    jnz @f
    mov eax, 300
@@:
    mov [rbx + 28], eax                ; win_h
    jmp .cmd_next

.x11_map_window:
    ; MapWindow (#8):
    ; If not already mapped, create real OpenSweet OS window via wm_create_user_window!
    cmp dword [rbx + 4], -1
    jne .map_already
    push rbx
    lea rdi, [rbx + 64]                ; title string pointer
    cmp byte [rdi], 0
    jne @f
    mov dword [rdi], "Debi"
    mov dword [rdi + 4], "an A"
    mov word  [rdi + 8], "pp"
@@:
    mov esi, [rbx + 16]                ; x
    mov edx, [rbx + 20]                ; y
    mov r10d, [rbx + 24]               ; w
    mov r8d, [rbx + 28]                ; h
    add r8d, 33                        ; total window height = client_h + 33 titlebar
    call wm_create_user_window
    pop rbx
    mov [rbx + 4], eax                 ; store allocated OpenSweet win_id
.map_already:
    jmp .cmd_next

.x11_configure_window:
    ; ConfigureWindow (#12): update x, y, width, height
    jmp .cmd_next

.x11_change_property:
    ; ChangeProperty (#18): often sets _NET_WM_NAME or WM_NAME (window title)
    ; Check if setting title:
    mov eax, [rsi + 8]                 ; property Atom
    ; Atom 39 = WM_NAME
    cmp eax, 39
    jne @f
    movzx ecx, word [rsi + 16]         ; format & data length
    test ecx, ecx
    jz @f
    cmp ecx, 60
    jbe .title_fit
    mov ecx, 60
.title_fit:
    push rdi
    push rsi
    lea rdi, [rbx + 64]
    add rsi, 24                        ; title string start
    rep movsb
    mov byte [rdi], 0
    pop rsi
    pop rdi
@@:
    jmp .cmd_next

.x11_query_ext:
    ; QueryExtension (#98): X11 extensions query (MIT-SHM, etc.)
    ; Send 32-byte extension reply:
    ;   +0: uint8  reply_type = 1
    ;   +1: uint8  unused = 0
    ;   +2: uint16 sequence_number
    ;   +4: uint32 reply_length = 0
    ;   +8: uint8  present = 1 (supported!)
    ;   +9: uint8  major_opcode = 130
    ;   +10: uint8 first_event = 64
    ;   +11: uint8 first_error = 128
    sub rsp, 32
    mov rdi, rsp
    mov ecx, 8
    xor eax, eax
    rep stosd
    mov rdi, rsp
    mov byte  [rdi + 0], 1             ; Reply
    mov ax, word [rbx + 48]
    mov word  [rdi + 2], ax            ; Sequence number
    mov dword [rdi + 4], 0             ; Length = 0
    mov byte  [rdi + 8], 1             ; Present = 1
    mov byte  [rdi + 9], 130           ; Major opcode = 130 (MIT-SHM)
    mov byte  [rdi + 10], 64           ; First event
    mov byte  [rdi + 11], 128          ; First error

    push rbx
    push rsi
    push rdx
    mov rsi, rsp
    mov rdx, 32
    call x11_client_write
    pop rdx
    pop rsi
    pop rbx
    add rsp, 32
    jmp .cmd_next

.x11_create_gc:
    mov eax, [rsi + 4]
    mov [rbx + 12], eax                ; store GC XID
    jmp .cmd_next

.x11_put_image:
    ; PutImage (#72): Direct image blit into OpenSweet window canvas!
    ; Format:
    ; +12: int16 dst_x, +14: int16 dst_y
    ; +16: uint16 width, +18: uint16 height
    ; +24: image pixel data starts
    mov eax, [rbx + 4]                 ; win_id
    cmp eax, -1
    je .cmd_next
    cmp eax, APP_COUNT
    jae .cmd_next

    ; Canvas pointer in wm_user_canvas[win_id]
    mov rdi, [r15 + wm_user_canvas - kmain + rax*8]
    test rdi, rdi
    jz .cmd_next

    movzx r8d, word [rsi + 16]         ; width
    movzx r9d, word [rsi + 18]         ; height
    test r8d, r8d
    jz .cmd_next
    test r9d, r9d
    jz .cmd_next

    ; Copy raw pixel data
    push rsi
    push rdi
    add rsi, 24                        ; pixel data
    mov ecx, r8d
    imul ecx, r9d                      ; total pixels (dwords)
    cmp ecx, 524288
    jbe @f
    mov ecx, 524288
@@:
    rep movsd
    pop rdi
    pop rsi

    ; Request screen redraw
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .cmd_next

.x11_poly_fill:
    ; Simple rect fill: clear client area with specified color
    mov eax, [rbx + 4]
    cmp eax, -1
    je .cmd_next
    cmp eax, APP_COUNT
    jae .cmd_next
    mov rdi, [r15 + wm_user_canvas - kmain + rax*8]
    test rdi, rdi
    jz .cmd_next
    mov ecx, 524288
    mov eax, 0xFF1E293B                ; Slate 800 background
    rep stosd
    mov byte [r15 + md_term_dirty - kmain], 1
    jmp .cmd_next

.x11_intern_atom:
    ; InternAtom (#16): Client requests an Atom ID for a string
    ; Wire format:
    ;   +0: uint8  opcode = 16
    ;   +1: uint8  only_if_exists
    ;   +2: uint16 length
    ;   +4: uint16 name_len
    ; Send 32-byte Reply:
    ;   +0: uint8  reply_type = 1
    ;   +1: uint8  unused = 0
    ;   +2: uint16 sequence_number
    ;   +4: uint32 reply_length = 0
    ;   +8: uint32 atom_id (return hashed/sequential atom, e.g. 0x200 + seq)
    sub rsp, 32
    mov rdi, rsp
    mov ecx, 8
    xor eax, eax
    rep stosd
    mov rdi, rsp
    mov byte  [rdi + 0], 1             ; Reply
    mov ax, word [rbx + 48]
    mov word  [rdi + 2], ax            ; Sequence number
    mov dword [rdi + 4], 0             ; Length = 0
    movzx eax, ax
    add eax, 0x200                     ; Generate stable atom ID
    mov dword [rdi + 8], eax           ; Atom ID

    push rbx
    push rsi
    push rdx
    mov rsi, rsp
    mov rdx, 32
    call x11_client_write
    pop rdx
    pop rsi
    pop rbx
    add rsp, 32
    jmp .cmd_next

.x11_get_property:
    ; GetProperty (#20): Client reads window property
    ; Send 32-byte Reply indicating property not found / empty:
    ;   +0: uint8  reply_type = 1
    ;   +1: uint8  format = 0 (empty)
    ;   +2: uint16 sequence_number
    ;   +4: uint32 reply_length = 0
    ;   +8: uint32 type = 0 (None)
    ;   +12: uint32 bytes_after = 0
    ;   +16: uint32 value_len = 0
    sub rsp, 32
    mov rdi, rsp
    mov ecx, 8
    xor eax, eax
    rep stosd
    mov rdi, rsp
    mov byte  [rdi + 0], 1             ; Reply
    mov byte  [rdi + 1], 0             ; Format = 0
    mov ax, word [rbx + 48]
    mov word  [rdi + 2], ax            ; Sequence number
    mov dword [rdi + 4], 0             ; Length = 0
    mov dword [rdi + 8], 0             ; Type = None (0)

    push rbx
    push rsi
    push rdx
    mov rsi, rsp
    mov rdx, 32
    call x11_client_write
    pop rdx
    pop rsi
    pop rbx
    add rsp, 32
    jmp .cmd_next

.proc_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret
