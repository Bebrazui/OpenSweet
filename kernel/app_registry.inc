; ==============================================================================
; Opensweet OS - Application Package Registry (app_registry.inc)
; Manages application metadata: Name, Version, Digital Signature / Author,
; Description, 44x44 Icon, Executable Path, and Dynamic Dock Integration.
; ==============================================================================

MAX_REG_APPS        = 8
APP_ENTRY_SIZE      = 160

; Field Offsets in App Registry Entry:
REG_APP_ID          = 0    ; byte (0..7)
REG_APP_INSTALLED   = 1    ; byte (0 = hidden, 1 = installed in dock)
REG_APP_NAME        = 2    ; 24 bytes (ASCIIZ)
REG_APP_VER         = 26   ; 12 bytes (ASCIIZ)
REG_APP_AUTH        = 38   ; 32 bytes (ASCIIZ) - Signature / Author
REG_APP_DESC        = 70   ; 48 bytes (ASCIIZ)
REG_APP_PATH        = 118  ; 32 bytes (ASCIIZ)
REG_APP_ICON_ID     = 150  ; word
REG_APP_ICON_PTR    = 152  ; qword (pointer to 44x44 ARGB pixel array)

align 16
app_registry_data rb MAX_REG_APPS * APP_ENTRY_SIZE

; Default string tables
str_reg_term_name db "Terminal", 0
str_reg_term_ver  db "1.0.0", 0
str_reg_term_auth db "OpenSweet Core", 0
str_reg_term_desc db "Interactive Shell Terminal", 0

str_reg_file_name db "Files", 0
str_reg_file_ver  db "1.0.0", 0
str_reg_file_auth db "OpenSweet Core", 0
str_reg_file_desc db "ext4 File Explorer", 0

str_reg_sys_name  db "Activity", 0
str_reg_sys_ver   db "1.0.0", 0
str_reg_sys_auth  db "OpenSweet Core", 0
str_reg_sys_desc  db "System Resource Monitor", 0

str_reg_set_name  db "Settings", 0
str_reg_set_ver   db "1.0.0", 0
str_reg_set_auth  db "OpenSweet Core", 0
str_reg_set_desc  db "OS Personalization & Settings", 0

str_reg_np_name   db "Notepad", 0
str_reg_np_ver    db "1.0.0", 0
str_reg_np_auth   db "OpenSweet Team [Verified]", 0
str_reg_np_desc   db "Modern GUI Text Editor", 0
str_reg_np_path   db "/notepad.elf", 0

str_reg_calc_name db "Calculator", 0
str_reg_calc_ver  db "1.0.0", 0
str_reg_calc_auth db "OpenSweet Team [Verified]", 0
str_reg_calc_desc db "GUI Desktop Calculator", 0
str_reg_calc_path db "/calc.elf", 0

; ==============================================================================
; app_reg_init: Initialize default registry entries
; ==============================================================================
app_reg_init:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    ; Clear entire registry
    lea rdi, [r15 + app_registry_data - kmain]
    mov ecx, (MAX_REG_APPS * APP_ENTRY_SIZE) / 8
    xor eax, eax
    cld
    rep stosq

    ; --- Entry 0: Terminal ---
    lea rdi, [r15 + app_registry_data - kmain + 0 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_ID], 0
    mov byte [rdi + REG_APP_INSTALLED], 1
    lea rsi, [r15 + str_reg_term_name - kmain]
    lea rdx, [rdi + REG_APP_NAME]
    call .copy_str24
    lea rsi, [r15 + str_reg_term_ver - kmain]
    lea rdx, [rdi + REG_APP_VER]
    call .copy_str12
    lea rsi, [r15 + str_reg_term_auth - kmain]
    lea rdx, [rdi + REG_APP_AUTH]
    call .copy_str32
    lea rsi, [r15 + str_reg_term_desc - kmain]
    lea rdx, [rdi + REG_APP_DESC]
    call .copy_str48
    mov word [rdi + REG_APP_ICON_ID], 0
    lea rax, [r15 + icon_terminal_44x44 - kmain]
    mov [rdi + REG_APP_ICON_PTR], rax

    ; --- Entry 1: Files ---
    lea rdi, [r15 + app_registry_data - kmain + 1 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_ID], 1
    mov byte [rdi + REG_APP_INSTALLED], 1
    lea rsi, [r15 + str_reg_file_name - kmain]
    lea rdx, [rdi + REG_APP_NAME]
    call .copy_str24
    lea rsi, [r15 + str_reg_file_ver - kmain]
    lea rdx, [rdi + REG_APP_VER]
    call .copy_str12
    lea rsi, [r15 + str_reg_file_auth - kmain]
    lea rdx, [rdi + REG_APP_AUTH]
    call .copy_str32
    lea rsi, [r15 + str_reg_file_desc - kmain]
    lea rdx, [rdi + REG_APP_DESC]
    call .copy_str48
    mov word [rdi + REG_APP_ICON_ID], 1
    lea rax, [r15 + icon_files_44x44 - kmain]
    mov [rdi + REG_APP_ICON_PTR], rax

    ; --- Entry 2: Activity (Sys) ---
    lea rdi, [r15 + app_registry_data - kmain + 2 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_ID], 2
    mov byte [rdi + REG_APP_INSTALLED], 1
    lea rsi, [r15 + str_reg_sys_name - kmain]
    lea rdx, [rdi + REG_APP_NAME]
    call .copy_str24
    lea rsi, [r15 + str_reg_sys_ver - kmain]
    lea rdx, [rdi + REG_APP_VER]
    call .copy_str12
    lea rsi, [r15 + str_reg_sys_auth - kmain]
    lea rdx, [rdi + REG_APP_AUTH]
    call .copy_str32
    lea rsi, [r15 + str_reg_sys_desc - kmain]
    lea rdx, [rdi + REG_APP_DESC]
    call .copy_str48
    mov word [rdi + REG_APP_ICON_ID], 2
    lea rax, [r15 + icon_sys_44x44 - kmain]
    mov [rdi + REG_APP_ICON_PTR], rax

    ; --- Entry 3: Settings ---
    lea rdi, [r15 + app_registry_data - kmain + 3 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_ID], 3
    mov byte [rdi + REG_APP_INSTALLED], 1
    lea rsi, [r15 + str_reg_set_name - kmain]
    lea rdx, [rdi + REG_APP_NAME]
    call .copy_str24
    lea rsi, [r15 + str_reg_set_ver - kmain]
    lea rdx, [rdi + REG_APP_VER]
    call .copy_str12
    lea rsi, [r15 + str_reg_set_auth - kmain]
    lea rdx, [rdi + REG_APP_AUTH]
    call .copy_str32
    lea rsi, [r15 + str_reg_set_desc - kmain]
    lea rdx, [rdi + REG_APP_DESC]
    call .copy_str48
    mov word [rdi + REG_APP_ICON_ID], 3
    lea rax, [r15 + icon_settings_44x44 - kmain]
    mov [rdi + REG_APP_ICON_PTR], rax

    ; --- Entry 4: Notepad (Pre-configured, installed = 0 until user install) ---
    lea rdi, [r15 + app_registry_data - kmain + 4 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_ID], 4
    mov byte [rdi + REG_APP_INSTALLED], 0
    lea rsi, [r15 + str_reg_np_name - kmain]
    lea rdx, [rdi + REG_APP_NAME]
    call .copy_str24
    lea rsi, [r15 + str_reg_np_ver - kmain]
    lea rdx, [rdi + REG_APP_VER]
    call .copy_str12
    lea rsi, [r15 + str_reg_np_auth - kmain]
    lea rdx, [rdi + REG_APP_AUTH]
    call .copy_str32
    lea rsi, [r15 + str_reg_np_desc - kmain]
    lea rdx, [rdi + REG_APP_DESC]
    call .copy_str48
    lea rsi, [r15 + str_reg_np_path - kmain]
    lea rdx, [rdi + REG_APP_PATH]
    call .copy_str32
    mov word [rdi + REG_APP_ICON_ID], 4
    lea rax, [r15 + icon_notepad_44x44 - kmain]
    mov [rdi + REG_APP_ICON_PTR], rax

    ; --- Entry 5: Calculator (Pre-configured, installed = 0 until user install) ---
    lea rdi, [r15 + app_registry_data - kmain + 5 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_ID], 5
    mov byte [rdi + REG_APP_INSTALLED], 0
    lea rsi, [r15 + str_reg_calc_name - kmain]
    lea rdx, [rdi + REG_APP_NAME]
    call .copy_str24
    lea rsi, [r15 + str_reg_calc_ver - kmain]
    lea rdx, [rdi + REG_APP_VER]
    call .copy_str12
    lea rsi, [r15 + str_reg_calc_auth - kmain]
    lea rdx, [rdi + REG_APP_AUTH]
    call .copy_str32
    lea rsi, [r15 + str_reg_calc_desc - kmain]
    lea rdx, [rdi + REG_APP_DESC]
    call .copy_str48
    lea rsi, [r15 + str_reg_calc_path - kmain]
    lea rdx, [rdi + REG_APP_PATH]
    call .copy_str32
    mov word [rdi + REG_APP_ICON_ID], 5
    lea rax, [r15 + icon_calc_44x44 - kmain]
    mov [rdi + REG_APP_ICON_PTR], rax

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

.copy_str24:
    push rcx
    push rdi
    mov rdi, rdx
    mov ecx, 23
    call .copy_loop
    pop rdi
    pop rcx
    ret

.copy_str12:
    push rcx
    push rdi
    mov rdi, rdx
    mov ecx, 11
    call .copy_loop
    pop rdi
    pop rcx
    ret

.copy_str32:
    push rcx
    push rdi
    mov rdi, rdx
    mov ecx, 31
    call .copy_loop
    pop rdi
    pop rcx
    ret

.copy_str48:
    push rcx
    push rdi
    mov rdi, rdx
    mov ecx, 47
    call .copy_loop
    pop rdi
    pop rcx
    ret

.copy_loop:
@@:
    lodsb
    stosb
    test al, al
    jz @f
    loop @b
@@:
    mov byte [rdi], 0
    ret

; ==============================================================================
; app_reg_count_installed: Count number of applications currently in Dock
; Returns: EAX = count (e.g. 4..8)
; ==============================================================================
app_reg_count_installed:
    push rbx
    push rcx
    push rdi

    xor eax, eax
    xor ecx, ecx
.count_loop:
    cmp ecx, MAX_REG_APPS
    jae .count_done
    imul ebx, ecx, APP_ENTRY_SIZE
    lea rdi, [r15 + app_registry_data - kmain + rbx]
    cmp byte [rdi + REG_APP_INSTALLED], 1
    jne @f
    inc eax
@@:
    inc ecx
    jmp .count_loop

.count_done:
    pop rdi
    pop rcx
    pop rbx
    ret

; ==============================================================================
; app_reg_get_nth_installed: Get registry entry for the EBX-th installed app
; EBX = index (0..installed_count - 1)
; Returns:
;   RDI = pointer to registry entry
;   EAX = app_id (0..7)
;   (or RDI = 0, EAX = -1 if not found)
; ==============================================================================
app_reg_get_nth_installed:
    push rbx
    push rcx
    push rdx

    mov edx, ebx                      ; target index
    xor ecx, ecx                      ; current slot (0..7)
    xor ebx, ebx                      ; current installed counter

.search_loop:
    cmp ecx, MAX_REG_APPS
    jae .not_found

    imul eax, ecx, APP_ENTRY_SIZE
    lea rdi, [r15 + app_registry_data - kmain + rax]
    cmp byte [rdi + REG_APP_INSTALLED], 1
    jne .next_slot

    cmp ebx, edx
    je .found_slot

    inc ebx
.next_slot:
    inc ecx
    jmp .search_loop

.found_slot:
    movzx eax, byte [rdi + REG_APP_ID]
    pop rdx
    pop rcx
    pop rbx
    ret

.not_found:
    xor edi, edi
    mov eax, -1
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; app_reg_install_by_path: Mark an application as installed by its exec_path or name
; RSI = path or name string (ASCIIZ)
; Returns: EAX = slot (0..7), or -1 if not found
; ==============================================================================
app_reg_install_by_path:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12

    mov r12, rsi                      ; search string

    ; Check slot 4 (Notepad): matches "/notepad.elf", "notepad.elf", "notepad", "Notepad"
    mov rsi, r12
    call .is_notepad_match
    test al, al
    jz .check_calc

    lea rdi, [r15 + app_registry_data - kmain + 4 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_INSTALLED], 1
    mov byte [r15 + md_term_dirty - kmain], 1
    mov eax, 4
    jmp .install_done

.check_calc:
    mov rsi, r12
    call .is_calc_match
    test al, al
    jz .search_generic

    lea rdi, [r15 + app_registry_data - kmain + 5 * APP_ENTRY_SIZE]
    mov byte [rdi + REG_APP_INSTALLED], 1
    mov byte [r15 + md_term_dirty - kmain], 1
    mov eax, 5
    jmp .install_done

.search_generic:
    xor ecx, ecx
.gen_loop:
    cmp ecx, MAX_REG_APPS
    jae .gen_not_found
    imul eax, ecx, APP_ENTRY_SIZE
    lea rdi, [r15 + app_registry_data - kmain + rax]

    ; Compare path
    push rcx
    push rdi
    push rsi
    mov rsi, r12
    lea rdi, [rdi + REG_APP_PATH]
    call streq
    pop rsi
    pop rdi
    pop rcx
    test al, al
    jnz .match_slot

    ; Compare name
    push rcx
    push rdi
    push rsi
    mov rsi, r12
    lea rdi, [rdi + REG_APP_NAME]
    call streq
    pop rsi
    pop rdi
    pop rcx
    test al, al
    jnz .match_slot

    inc ecx
    jmp .gen_loop

.match_slot:
    mov byte [rdi + REG_APP_INSTALLED], 1
    mov byte [r15 + md_term_dirty - kmain], 1
    movzx eax, byte [rdi + REG_APP_ID]
    jmp .install_done

.gen_not_found:
    mov eax, -1

.install_done:
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.is_notepad_match:
    push rsi
    push rdi
    ; match "/notepad.elf"
    lea rdi, [r15 + str_reg_np_path - kmain]
    call streq
    test al, al
    jnz .np_yes
    ; match "notepad.elf"
    pop rdi
    pop rsi
    push rsi
    push rdi
    cmp byte [rsi], '/'
    jne @f
    inc rsi
@@:
    lea rdi, [r15 + str_reg_np_name - kmain] ; "Notepad"
    call streq
    test al, al
    jnz .np_yes

    xor al, al
    pop rdi
    pop rsi
    ret
.np_yes:
    mov al, 1
    pop rdi
    pop rsi
    ret

.is_calc_match:
    push rsi
    push rdi
    lea rdi, [r15 + str_reg_calc_path - kmain]
    call streq
    test al, al
    jnz .calc_yes
    pop rdi
    pop rsi
    push rsi
    push rdi
    cmp byte [rsi], '/'
    jne @f
    inc rsi
@@:
    lea rdi, [r15 + str_reg_calc_name - kmain] ; "Calculator"
    call streq
    test al, al
    jnz .calc_yes

    xor al, al
    pop rdi
    pop rsi
    ret
.calc_yes:
    mov al, 1
    pop rdi
    pop rsi
    ret

; ==============================================================================
; app_reg_install_from_user: Register/install an app from Ring 3 user metadata
; RDI = user pointer to os_app_info_t (struct size ~160)
; RSI = size in bytes
; Returns: EAX = 0 on success, -1 on error
; ==============================================================================
app_reg_install_from_user:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13

    ; 1. Validate user pointer
    call sys_validate_user_ptr
    jc .err_ret

    mov r12, rdi                      ; r12 = user struct ptr

    ; 2. Determine slot: check if name is "Notepad" -> slot 4, "Calculator" -> slot 5
    mov ebx, 4                        ; default target slot
    cmp byte [r12], 'C'               ; "Calculator"?
    jne @f
    mov ebx, 5
@@:
    cmp byte [r12], 'c'
    jne @f
    mov ebx, 5
@@:

    imul eax, ebx, APP_ENTRY_SIZE
    lea r13, [r15 + app_registry_data - kmain + rax]

    ; 3. Copy name (24 bytes)
    push rsi
    push rdi
    mov rsi, r12                      ; user.name
    lea rdi, [r13 + REG_APP_NAME]
    mov ecx, 23
@@:
    lodsb
    stosb
    test al, al
    jz @f
    loop @b
@@:
    mov byte [rdi], 0
    pop rdi
    pop rsi

    ; 4. Copy version (offset 24, 12 bytes)
    push rsi
    push rdi
    lea rsi, [r12 + 24]
    lea rdi, [r13 + REG_APP_VER]
    mov ecx, 11
@@:
    lodsb
    stosb
    test al, al
    jz @f
    loop @b
@@:
    mov byte [rdi], 0
    pop rdi
    pop rsi

    ; 5. Copy author/signature (offset 36, 32 bytes)
    push rsi
    push rdi
    lea rsi, [r12 + 36]
    lea rdi, [r13 + REG_APP_AUTH]
    mov ecx, 31
@@:
    lodsb
    stosb
    test al, al
    jz @f
    loop @b
@@:
    mov byte [rdi], 0
    pop rdi
    pop rsi

    ; 6. Copy description (offset 68, 48 bytes)
    push rsi
    push rdi
    lea rsi, [r12 + 68]
    lea rdi, [r13 + REG_APP_DESC]
    mov ecx, 47
@@:
    lodsb
    stosb
    test al, al
    jz @f
    loop @b
@@:
    mov byte [rdi], 0
    pop rdi
    pop rsi

    ; 7. Copy exec_path (offset 116, 32 bytes)
    push rsi
    push rdi
    lea rsi, [r12 + 116]
    lea rdi, [r13 + REG_APP_PATH]
    mov ecx, 31
@@:
    lodsb
    stosb
    test al, al
    jz @f
    loop @b
@@:
    mov byte [rdi], 0
    pop rdi
    pop rsi

    ; 8. Set icon
    mov edx, [r12 + 148]              ; user.icon_id
    mov [r13 + REG_APP_ICON_ID], dx
    cmp edx, 4                        ; OS_ICON_NOTEPAD
    jne @f
    lea rax, [r15 + icon_notepad_44x44 - kmain]
    mov [r13 + REG_APP_ICON_PTR], rax
    jmp .icon_set
@@:
    cmp edx, 5                        ; OS_ICON_CALC
    jne @f
    lea rax, [r15 + icon_calc_44x44 - kmain]
    mov [r13 + REG_APP_ICON_PTR], rax
    jmp .icon_set
@@:
    ; Default to notepad icon if slot 4, calc if slot 5
    cmp ebx, 4
    jne @f
    lea rax, [r15 + icon_notepad_44x44 - kmain]
    mov [r13 + REG_APP_ICON_PTR], rax
    jmp .icon_set
@@:
    lea rax, [r15 + icon_calc_44x44 - kmain]
    mov [r13 + REG_APP_ICON_PTR], rax

.icon_set:
    ; 9. Mark installed and trigger desktop redraw!
    mov byte [r13 + REG_APP_INSTALLED], 1
    mov byte [r15 + md_term_dirty - kmain], 1

    xor eax, eax                      ; return 0 (success)
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.err_ret:
    mov eax, -1
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret
