@echo off
setlocal
cd /d %~dp0
set PATH=%~dp0tools\w64devkit\bin;%PATH%
set GCC=tools\w64devkit\bin\gcc.exe
set OBJCOPY=tools\w64devkit\bin\objcopy.exe
set INC=-Iinclude -Iuser\doomgeneric\doomgeneric
set CFLAGS=-std=gnu99 -mabi=sysv -O2 -DNORMALUNIX -DLINUX -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -fno-pic -fno-asynchronous-unwind-tables -fno-exceptions -mno-red-zone -Wall -Wno-unused-variable -Wno-unused-function -Wno-format -D_WIN64

if not exist build\doom_objs mkdir build\doom_objs

echo Compiling user\doom_libc.c...
%GCC% %INC% %CFLAGS% -c user\doom_libc.c -o build\doom_objs\doom_libc.o
if errorlevel 1 exit /b 1

echo Compiling user\doomgeneric_opensweet.c...
%GCC% %INC% %CFLAGS% -c user\doomgeneric_opensweet.c -o build\doom_objs\doomgeneric_opensweet.o
if errorlevel 1 exit /b 1

set SRC_FILES=dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric

for %%f in (%SRC_FILES%) do (
    if not exist build\doom_objs\%%f.o (
        echo Compiling %%f.c...
        %GCC% %INC% %CFLAGS% -c user\doomgeneric\doomgeneric\%%f.c -o build\doom_objs\%%f.o
        if errorlevel 1 exit /b 1
    )
)

echo Linking build\doom.exe...
%GCC% -Btools\w64devkit\bin %INC% -mabi=sysv -nostdlib -Wl,--image-base=0x400000 -O2 user\crt0.s build\doom_objs\*.o -o build\doom.exe
if errorlevel 1 exit /b 1

echo Converting to build\doom.elf...
%OBJCOPY% -O elf64-x86-64 build\doom.exe build\doom.elf
if errorlevel 1 exit /b 1

del build\doom.exe >nul 2>&1
echo DOOM Build OK: build\doom.elf
