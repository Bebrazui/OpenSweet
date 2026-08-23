# Ext4 read-only: наш дизайн (чистая комната)

Референс для изучения: KolibriOS `kernel/trunk/fs/ext.inc` (GPLv2, локальная копия
`build/_kolibri_ext.inc`, НЕ коммитится). Кода не копируем — ниже конспект формата
ext4 по публичной документации (kernel.org docs) и наш план реализации.

## Что поддерживаем (read-only, v1)

- Блочный размер: 1KB..64KB (`1024 << log_block_size`)
- Фичи incompatible, которые разрешаем: `FILETYPE(0x2) | EXTENTS(0x40) | 64BIT(0x80) |
  FLEX_BG(0x200)`. Всё остальное в `s_feature_incompat` -> отказ монтирования.
- Чтение файлов: extent-дерево (современный ext4) + fallback на indirect-блоки
- Каталоги: линейные dir entries (без htree-индексации — htree это ускоритель,
  линейный скан корректен всегда)
- Символические ссылки: fast symlink (цель в inode) + обычные

Не трогаем: журнал (не реплеим, просто игнорируем), шифрование, verity, casefold.

## Формат на диске (каноничные оффсеты)

### Superblock — байт 1024 от начала раздела, 1024 байта

| Оффсет | Размер | Поле | Что проверяем |
|--------|--------|------|---------------|
| 0x38   | u16    | magic = 0xEF53 | обязательно |
| 0x36   | u16    | state (1 = clean) | предупреждение, не фейл |
| 0x18   | u32    | log_block_size | blockSize = 1024 << значение |
| 0x20   | u32    | blocks_per_group | != 0 |
| 0x10   | u32    | inodes_per_group | != 0 |
| 0x54   | u32    | first_ino (rev>=1) | первый нон-резервный inode |
| 0x58   | u16    | inode_size (rev>=1; иначе 128) | 128..512, кратно 4 |
| 0x5C   | u32    | feature_compat | игнорируем |
| 0x60   | u32    | feature_incompatible | маска выше |
| 0x64   | u32    | feature_ro_compat | игнорируем (gdt_csum не мешает читать) |

### Block groups / GDT

- group = (block - first_data_block) / blocks_per_group; first_data_block = 1 при 1KB блоках, иначе 0
- GDT начинается с блока `first_data_block + 1`
- Дескриптор группы (32B, legacy): `bg_block_bitmap_lo @0, bg_inode_bitmap_lo @4,
  bg_inode_table_lo @8` (u32)
- Inode lookup: `group = (ino - 1) / inodes_per_group`,
  `index = (ino - 1) % inodes_per_group`;
  адрес = inode_table_block * blockSize + index * inode_size

### Inode

```
mode u16, uid u16, size_lo u32, atime/ctime/mtime/dtime 4xu32, gid u16,
links u16, blocks_lo u32, flags u32 (@0x20), ... , block[15] u32 @0x28,
generation, file_acl_lo, size_hi/dir_acl @0x6C, ...
```

- Флаг extents: `flags & 0x80000`
- Тип файла: mode >> 12 & 0xF: 2=dir, 8=regular, 7=symlink (или filetype из dir entry)

### Extent-дерево (если flags & 0x80000)

Корень — прямо в inode.block[0..3]:
```
header: magic u16 = 0xF30A, entries u16, max u16, depth u16, generation u32  (12 bytes)
depth == 0: leaf записи EXTENT {fileBlock u32, fsBlockLo u32, len u16} по 12B
depth  > 0: INDEX записи {fileBlock u32, leafLo u32} по 8B;
            лист = блок с такой же структурой header+entries
Поиск логического блока L: идём по записям, берём последнюю с fileBlock <= L;
у index-записи читаем дочерний блок и рекурсивно.
Результат: физБлок = fsBlockLo + (L - fileBlock), длина прогона = len.
```

### Legacy indirect (fallback)

12 прямых в inode.block[0..11]; block[12] = indirect (u32 указателей на блоки),
block[13] = double, block[14] = triple. dwordsPerBlock = blockSize/4.

### Каталог

Последовательность записей внутри data-блоков каталога:
```
inode u32, rec_len u16, name_len u8, type u8, name[name_len]
```
- rec_len округляет запись до 4; inode==0 -> пустая дыра, пропускаем по rec_len
- конец блока: суммарный проход == blockSize
- Имена: до 255 байт, UTF-8

### Symlink

- fast: `(mode & 0xF000)==0xA000` и `blocks_lo == 0` -> цель лежит в block[0..]
- slow: цель — содержимое файла

## Наш интерфейс (fs/ext4/ext4.inc)

```asm
; ext4_mount: RAX = LBA начала раздела, RCX = кол-во секторов
;   -> RAX = 0 ок / код ошибки; структура EXT4_MOUNT в выделенной памяти
ext4_mount
ext4_read_dir     ; RCX=inode, буфер -> список имён (для shell `ls`)
ext4_lookup       ; путь ASCIIZ -> inode number
ext4_read_file    ; inode, offset, len, буфер -> прочитано байт
```

Всё поверх двух функций блочного слоя:
```asm
disk_read_blocks  ; RAX=LBA, RCX=count, RDI=буфер
disk_write_blocks ; (позже)
```

## Порядок работ

1. **ATA PIO driver** (drivers/ata.asm): identify, read sectors LBA28 polling.
   QEMU: `-drive format=raw,file=disk.img,if=ide` — второй диск.
2. **Образ**: mkfs.ext4 недоступен на Windows без WSL — генерируем минимальный
   образ скриптом (PowerShell/Python): superblock+GDT+root inode+пара файлов,
   extents-only. Это же будет юнит-тестом.
3. `fs/ext4/ext4.inc`: mount -> lookup -> read_file -> ls/cat команды в shell.
4. Потом: CRC (metadata_csum), htree, запись.

## Уроки из референса (что подсмотрели в подходе, но пишем сами)

- Валидировать magic/state/features ДО аллокации структур
- Один общий block buffer + отдельный inode buffer вместо кэша (минимализм;
  у нас позже появится кэш на VMM)
- Поиск в extent-узле: линейный скан "последняя запись с fileBlock <= L"
- Symlink depth limit обязателен (защита от петель)
