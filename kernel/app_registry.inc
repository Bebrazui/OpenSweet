; ==============================================================================
; Opensweet OS - Application Package Registry (app_registry.inc)
; Manages application metadata: Name, Version, Digital Signature / Author,
; Description, 44x44 Icon, Executable Path, and Dynamic Dock Integration.
; ALL user app icons & metadata are embedded inside user ELF binaries (.os_app).
; ZERO user application icons are hardcoded into the kernel!
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

OS_APP_MAGIC        = 0x00005050415F534F ; "OS_APP\0\0"

align 16
app_registry_data rb MAX_REG_APPS * APP_ENTRY_SIZE

align 16
app_custom_icons  rb MAX_REG_APPS * (44 * 44 * 4)

align 16
str_inst_path_buf rb 64

; Default Core OS application string tables
str_reg_term_name db "Terminal", 0
str_reg_term_ver  db "1.0.0", 0
str_reg_term_auth db "OpenSweet Core", 0
str_reg_term_desc db "Interactive Shell Terminal", 0
str_reg_term_path db "/terminal.elf", 0

str_reg_file_name db "Files", 0
str_reg_file_ver  db "1.0.0", 0
str_reg_file_auth db "OpenSweet Core", 0
str_reg_file_desc db "ext4 File Explorer", 0
str_reg_file_path db "/files.elf", 0

str_reg_sys_name  db "Activity", 0
str_reg_sys_ver   db "1.0.0", 0
str_reg_sys_auth  db "OpenSweet Core", 0
str_reg_sys_desc  db "System Resource Monitor", 0

str_reg_set_name  db "Settings", 0
str_reg_set_ver   db "1.0.0", 0
str_reg_set_auth  db "OpenSweet Core", 0
str_reg_set_desc  db "OS Personalization & Settings", 0

; ==============================================================================
; app_reg_init: Initialize default core system registry entries
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

    ; Clear custom icon cache
    lea rdi, [r15 + app_custom_icons - kmain]
    mov ecx, (MAX_REG_APPS * (44 * 44 * 4)) / 8
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
    lea rsi, [r15 + str_reg_term_path - kmain]
    lea rdx, [rdi + REG_APP_PATH]
    call .copy_str32
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
    lea rsi, [r15 + str_reg_file_path - kmain]
    lea rdx, [rdi + REG_APP_PATH]
    call .copy_str32
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

    ; Slots 4..7 are dynamic user app slots populated from ELF files!

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
; Returns: EAX = count (1..8)
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
; app_reg_find_or_alloc_slot: Find existing slot by path/name or allocate free slot
; RSI = search path or name (ASCIIZ)
; Returns: EAX = slot index (4..7), or -1 if full
; ==============================================================================
app_reg_find_or_alloc_slot:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12

    mov r12, rsi

    ; 1. Check if an installed user slot already matches path or name
    mov ecx, 4
.match_loop:
    cmp ecx, MAX_REG_APPS
    jae .find_free
    imul eax, ecx, APP_ENTRY_SIZE
    lea rdi, [r15 + app_registry_data - kmain + rax]
    cmp byte [rdi + REG_APP_INSTALLED], 1
    jne .match_next

    ; Check path
    push rcx
    push rdi
    mov rsi, r12
    lea rdi, [rdi + REG_APP_PATH]
    call streq
    pop rdi
    pop rcx
    test al, al
    jnz .slot_matched

    ; Check name
    push rcx
    push rdi
    mov rsi, r12
    lea rdi, [rdi + REG_APP_NAME]
    call streq
    pop rdi
    pop rcx
    test al, al
    jnz .slot_matched

.match_next:
    inc ecx
    jmp .match_loop

.slot_matched:
    mov eax, ecx
    jmp .alloc_done

.find_free:
    ; 2. Pick first free slot in 4..7
    mov ecx, 4
.free_loop:
    cmp ecx, MAX_REG_APPS
    jae .no_slot
    imul eax, ecx, APP_ENTRY_SIZE
    lea rdi, [r15 + app_registry_data - kmain + rax]
    cmp byte [rdi + REG_APP_INSTALLED], 0
    je .slot_free
    inc ecx
    jmp .free_loop

.slot_free:
    mov eax, ecx
    jmp .alloc_done

.no_slot:
    mov eax, -1

.alloc_done:
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; app_reg_install_from_user: Register/install an app from Ring 3 user metadata
; RDI = user pointer to os_app_package_t (starts with OS_APP_MAGIC)
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
    jc .uerr_ret

    mov r12, rdi                      ; r12 = user struct ptr

    ; 2. Validate magic ("OS_APP\0\0")
    mov rax, [r12]
    mov rbx, OS_APP_MAGIC
    cmp rax, rbx
    jne .uerr_ret

    ; 3. Validate icon dimensions (44x44)
    cmp dword [r12 + 156], 44
    jne .uerr_ret
    cmp dword [r12 + 160], 44
    jne .uerr_ret

    ; 4. Find existing slot or allocate new one in 4..7
    lea rsi, [r12 + 124]              ; user.exec_path
    cmp byte [rsi], 0
    jne @f
    lea rsi, [r12 + 8]                ; fallback to user.name
@@:
    call app_reg_find_or_alloc_slot
    cmp eax, -1
    je .uerr_ret

    mov ebx, eax                      ; ebx = target slot (4..7)
    imul eax, ebx, APP_ENTRY_SIZE
    lea r13, [r15 + app_registry_data - kmain + rax]

    mov [r13 + REG_APP_ID], bl

    ; 5. Copy name (offset 8, 24 bytes)
    lea rsi, [r12 + 8]
    lea rdx, [r13 + REG_APP_NAME]
    call app_reg_init.copy_str24

    ; 6. Copy version (offset 32, 12 bytes)
    lea rsi, [r12 + 32]
    lea rdx, [r13 + REG_APP_VER]
    call app_reg_init.copy_str12

    ; 7. Copy author/signature (offset 44, 32 bytes)
    lea rsi, [r12 + 44]
    lea rdx, [r13 + REG_APP_AUTH]
    call app_reg_init.copy_str32

    ; 8. Copy description (offset 76, 48 bytes)
    lea rsi, [r12 + 76]
    lea rdx, [r13 + REG_APP_DESC]
    call app_reg_init.copy_str48

    ; 9. Copy exec_path (offset 124, 32 bytes)
    lea rsi, [r12 + 124]
    lea rdx, [r13 + REG_APP_PATH]
    call app_reg_init.copy_str32

    ; 10. Copy 44x44 icon pixels from user struct directly into app_custom_icons
    imul eax, ebx, (44 * 44 * 4)
    lea rdx, [r15 + app_custom_icons - kmain + rax]

    push rsi
    push rdi
    lea rsi, [r12 + 164]              ; user icon_pixels array
    mov rdi, rdx                      ; kernel destination buffer
    mov ecx, (44 * 44)                ; 1936 dwords
    cld
    rep movsd
    pop rdi
    pop rsi

    mov [r13 + REG_APP_ICON_PTR], rdx
    mov word [r13 + REG_APP_ICON_ID], 0xFF

    ; 11. Mark installed in dock and trigger redraw!
    mov byte [r13 + REG_APP_INSTALLED], 1
    mov byte [r15 + md_term_dirty - kmain], 1

    xor eax, eax
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

.uerr_ret:
    mov eax, -1
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ret

; ==============================================================================
; app_reg_install_by_path: Extract package metadata & 44x44 icon from ELF binary
; RSI = path string (e.g. "/notepad.elf" or "notepad.elf")
; Returns: EAX = slot index (4..7), or -1 if failed
; ==============================================================================
app_reg_install_by_path:
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov r14, rsi                      ; r14 = original input path string

    ; 1. Check if ext4 is mounted
    cmp byte [r15 + ext4_ok - kmain], 0
    je .inst_fail

    ; 2. Normalize path into str_inst_path_buf (ensure leading '/')
    lea rdi, [r15 + str_inst_path_buf - kmain]
    mov rsi, r14
    cmp byte [rsi], '/'
    je .copy_norm_path
    mov byte [rdi], '/'
    inc rdi
.copy_norm_path:
    mov ecx, 62
@@:
    lodsb
    stosb
    test al, al
    jz @f
    loop @b
@@:
    mov byte [rdi], 0

    ; 3. Lookup inode on ext4
    lea rsi, [r15 + str_inst_path_buf - kmain]
    call ext4_lookup
    test eax, eax
    jz .inst_fail

    ; 4. Load inode
    call ext4_inode_load
    jc .inst_fail

    ; 5. Verify file size >= 8000 bytes (header + icon)
    mov rax, qword [r15 + ext4_inode - kmain + IN_OFF_SIZE]
    cmp rax, 8000
    jb .inst_fail
    cmp rax, 16*1024*1024             ; max 16MB
    ja .inst_fail
    mov r12, rax                      ; r12 = file size

    ; 6. Allocate buffer from Kernel Heap
    mov rcx, r12
    call kmalloc
    test rax, rax
    jz .inst_fail
    mov r13, rax                      ; r13 = ELF buffer pointer

    ; 7. Read file data from ext4 into allocated buffer
    lea rsi, [r15 + str_inst_path_buf - kmain]
    mov rdi, r13
    mov rcx, r12
    call ext4_read_file_to_mem
    jc .inst_free_fail

    ; 8. Search ELF file buffer for OS_APP_MAGIC (0x00005050415F534F)
    mov rsi, r13
    mov rcx, r12
    sub rcx, 164 + 7744               ; ensure full struct fits
    jbe .inst_free_fail

    mov rax, OS_APP_MAGIC
.scan_magic:
    cmp [rsi], rax
    je .found_elf_pkg
    inc rsi
    dec rcx
    jnz .scan_magic

    ; Magic not found in ELF
    jmp .inst_free_fail

.found_elf_pkg:
    ; RSI now points to embedded os_app_package_t inside the ELF file!
    ; Validate dimensions (44x44)
    cmp dword [rsi + 156], 44
    jne .inst_free_fail
    cmp dword [rsi + 160], 44
    jne .inst_free_fail

    ; Find or allocate slot in 4..7
    push rsi
    lea rsi, [r15 + str_inst_path_buf - kmain]
    call app_reg_find_or_alloc_slot
    pop rsi
    cmp eax, -1
    je .inst_free_fail

    mov ebx, eax                      ; ebx = target slot (4..7)
    imul eax, ebx, APP_ENTRY_SIZE
    lea rdi, [r15 + app_registry_data - kmain + rax]

    mov [rdi + REG_APP_ID], bl

    ; Copy name (offset 8, 24 bytes)
    push rsi
    push rdi
    lea rsi, [rsi + 8]
    lea rdx, [rdi + REG_APP_NAME]
    call app_reg_init.copy_str24
    pop rdi
    pop rsi

    ; Copy version (offset 32, 12 bytes)
    push rsi
    push rdi
    lea rsi, [rsi + 32]
    lea rdx, [rdi + REG_APP_VER]
    call app_reg_init.copy_str12
    pop rdi
    pop rsi

    ; Copy author/signature (offset 44, 32 bytes)
    push rsi
    push rdi
    lea rsi, [rsi + 44]
    lea rdx, [rdi + REG_APP_AUTH]
    call app_reg_init.copy_str32
    pop rdi
    pop rsi

    ; Copy description (offset 76, 48 bytes)
    push rsi
    push rdi
    lea rsi, [rsi + 76]
    lea rdx, [rdi + REG_APP_DESC]
    call app_reg_init.copy_str48
    pop rdi
    pop rsi

    ; Copy exec_path (offset 124, 32 bytes). If empty, use normalized path!
    push rsi
    push rdi
    cmp byte [rsi + 124], 0
    jne @f
    lea rsi, [r15 + str_inst_path_buf - kmain]
    lea rdx, [rdi + REG_APP_PATH]
    call app_reg_init.copy_str32
    jmp .path_copied
@@:
    lea rsi, [rsi + 124]
    lea rdx, [rdi + REG_APP_PATH]
    call app_reg_init.copy_str32
.path_copied:
    pop rdi
    pop rsi

    ; Copy 44x44 icon pixels from ELF file into app_custom_icons slot
    imul eax, ebx, (44 * 44 * 4)
    lea rdx, [r15 + app_custom_icons - kmain + rax]

    push rsi
    push rdi
    lea rsi, [rsi + 164]              ; offset of icon_pixels in os_app_package_t
    mov rdi, rdx
    mov ecx, (44 * 44)                ; 1936 dwords
    cld
    rep movsd
    pop rdi
    pop rsi

    mov [rdi + REG_APP_ICON_PTR], rdx
    mov word [rdi + REG_APP_ICON_ID], 0xFF

    ; Mark installed
    mov byte [rdi + REG_APP_INSTALLED], 1
    mov byte [r15 + md_term_dirty - kmain], 1

    ; Free temporary ELF heap buffer
    mov rdi, r13
    call kfree

    mov eax, ebx                      ; return slot number (4..7)
    jmp .inst_ret

.inst_free_fail:
    mov rdi, r13
    call kfree

.inst_fail:
    mov eax, -1

.inst_ret:
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
