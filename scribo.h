/*
 * scribo.h — declarationes scriptoris Mach-O
 */

#ifndef SCRIBO_H
#define SCRIBO_H

/* ================================================================
 * constantiae Mach-O
 * ================================================================ */

#define PAGINA          0x4000    /* ARM64 macOS pagina = 16 KiB */
#define VM_BASIS        0x100000000ULL

#define MH_MAGIC_64         0xFEEDFACF
#define MH_EXECUTE          2
#define CPU_TYPE_ARM64      (0x01000000 | 12)
#define CPU_SUBTYPE_ALL     0

#define LC_SEGMENT_64       0x19
#define LC_SYMTAB           0x02
#define LC_DYSYMTAB         0x0B
#define LC_LOAD_DYLINKER    0x0E
#define LC_LOAD_DYLIB       0x0C
#define LC_MAIN             (0x80000000 | 0x28)
#define LC_DYLD_INFO_ONLY   (0x80000000 | 0x22)
#define LC_BUILD_VERSION    0x32
#define LC_UUID             0x1B

#define MH_PIE              0x200000
#define MH_DYLDLINK         0x4
#define MH_TWOLEVEL         0x80
#define MH_NOUNDEFS         0x1

#define VM_PROT_NONE        0
#define VM_PROT_READ        1
#define VM_PROT_WRITE       2
#define VM_PROT_EXECUTE     4

#define S_REGULAR           0x0
#define S_CSTRING_LITERALS  0x2
#define S_ZEROFILL          0x1
#define S_NON_LAZY_SYMBOL_POINTERS 0x6
#define S_ATTR_PURE_INSTRUCTIONS   0x80000000
#define S_ATTR_SOME_INSTRUCTIONS   0x00000400

#define BIND_OPCODE_DONE                          0x00
#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM         0x10
#define BIND_OPCODE_SET_DYLIB_SPECIAL_IMM         0x30
#define BIND_OPCODE_SET_TYPE_IMM                  0x50
#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB   0x70
#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM 0x40
#define BIND_OPCODE_DO_BIND                       0x90

#define BIND_TYPE_POINTER   1

#define REBASE_OPCODE_DONE                        0x00
#define REBASE_OPCODE_SET_TYPE_IMM                0x10
#define REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB 0x20
#define REBASE_OPCODE_DO_REBASE_IMM_TIMES         0x50
#define REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIP   0x60

#define REBASE_TYPE_POINTER 1

#define N_EXT               0x01
#define N_UNDF              0x0
#define N_SECT              0xE

#define EXPORT_SYMBOL_FLAGS_KIND_REGULAR 0x00

#define PLATFORM_MACOS      1
#define TOOL_LD             3

/* ================================================================
 * declarationes
 * ================================================================ */

void scribo_macho(const char *plica_exitus, int main_offset);
void scribo_obiectum(const char *plica_exitus);

#endif /* SCRIBO_H */
