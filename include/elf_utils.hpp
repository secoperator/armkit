#pragma once
#include <cstdint>
#include <cstddef>
#include "syscall.hpp"
#include "mem_utils.hpp"

// Minimal ELF64 types so we don't pull in <elf.h> (which would drag libc headers).
using Elf64_Addr  = uint64_t;
using Elf64_Off   = uint64_t;
using Elf64_Half  = uint16_t;
using Elf64_Word  = uint32_t;
using Elf64_Sword = int32_t;
using Elf64_Xword = uint64_t;
using Elf64_Sxword= int64_t;

static constexpr int EI_NIDENT = 16;
static constexpr uint32_t ELF_MAGIC = 0x464c457f; // \x7fELF

// ELF program header types
static constexpr uint32_t PT_LOAD    = 1;
static constexpr uint32_t PT_DYNAMIC = 2;
static constexpr uint32_t PT_PHDR    = 6;

// ELF section header types
static constexpr uint32_t SHT_STRTAB  = 3;
static constexpr uint32_t SHT_SYMTAB  = 2;
static constexpr uint32_t SHT_DYNSYM  = 11;

// Dynamic array tags
static constexpr int64_t DT_NULL    = 0;
static constexpr int64_t DT_STRTAB  = 5;
static constexpr int64_t DT_SYMTAB  = 6;
static constexpr int64_t DT_STRSZ   = 10;
static constexpr int64_t DT_SYMENT  = 11;
static constexpr int64_t DT_HASH    = 4;
static constexpr int64_t DT_GNU_HASH= 0x6ffffef5;

struct Elf64_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off  e_phoff;
    Elf64_Off  e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
};

struct Elf64_Phdr {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
};

struct Elf64_Shdr {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
};

struct Elf64_Sym {
    Elf64_Word  st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half  st_shndx;
    Elf64_Addr  st_value;
    Elf64_Xword st_size;
};

struct Elf64_Dyn {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr  d_ptr;
    } d_un;
};

// /proc/self/maps line parser result
struct MapEntry {
    uint64_t start;
    uint64_t end;
    char     perms[5]; // rwxp + NUL
    char     path[256];
};

// Parse a single /proc/self/maps line into MapEntry.
// Returns true on success.
static inline bool parse_maps_line(const char* line, MapEntry& out) {
    // Format: start-end perms offset dev inode pathname
    const char* p = line;

    auto hex_byte = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    auto parse_hex = [&](uint64_t& val) -> bool {
        val = 0;
        bool any = false;
        while (true) {
            int d = hex_byte(*p);
            if (d < 0) break;
            val = val * 16 + (uint64_t)d;
            ++p; any = true;
        }
        return any;
    };

    if (!parse_hex(out.start)) return false;
    if (*p++ != '-') return false;
    if (!parse_hex(out.end))  return false;
    if (*p++ != ' ') return false;

    // perms: rwxp
    for (int i = 0; i < 4 && *p && *p != ' '; ++i) out.perms[i] = *p++;
    out.perms[4] = '\0';
    if (*p++ != ' ') return false;

    // skip offset, dev, inode
    for (int field = 0; field < 3; ++field) {
        while (*p && *p != ' ') ++p;
        while (*p == ' ') ++p;
    }

    // path (may be empty)
    out.path[0] = '\0';
    int i = 0;
    while (*p && *p != '\n' && i < 255) out.path[i++] = *p++;
    out.path[i] = '\0';

    return true;
}

// Result of a library lookup
struct LibInfo {
    uint64_t base;       // lowest load address (slide)
    bool     found;
};

// Read /proc/self/maps and find the base address of a loaded library.
// 'name' is matched as a suffix of the path field.
static LibInfo find_library_base(const char* name) {
    LibInfo result{0, false};

    // Open /proc/self/maps
    constexpr int O_RDONLY = 0;
    long fd = sys_open("/proc/self/maps", O_RDONLY);
    if (fd < 0) return result;

    // Read in chunks
    static char buf[4096];
    static char line[512];
    int line_pos = 0;

    long n;
    bool done = false;
    while (!done) {
        n = sys_read((int)fd, buf, sizeof(buf));
        if (n <= 0) break;

        for (long i = 0; i < n && !done; ++i) {
            char c = buf[i];
            if (c == '\n' || line_pos == 511) {
                line[line_pos] = '\0';
                line_pos = 0;

                MapEntry entry{};
                if (parse_maps_line(line, entry)) {
                    size_t plen = str_len(entry.path);
                    size_t nlen = str_len(name);
                    if (plen >= nlen &&
                        str_ncmp(entry.path + plen - nlen, name, nlen) == 0) {
                        result.base  = entry.start;
                        result.found = true;
                        done = true;
                    }
                }
            } else {
                line[line_pos++] = c;
            }
        }
    }

    sys_close((int)fd);
    return result;
}

// Walk the ELF loaded in memory at 'base' and look up a symbol by name.
// Returns the absolute address or 0 if not found.
static uint64_t find_symbol_in_memory(uint64_t base, const char* sym_name) {
    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(base);

    // Validate ELF magic
    if (*reinterpret_cast<const uint32_t*>(ehdr->e_ident) != ELF_MAGIC)
        return 0;

    // Walk program headers to find PT_DYNAMIC
    const auto* phdr = reinterpret_cast<const Elf64_Phdr*>(base + ehdr->e_phoff);
    const Elf64_Dyn* dyn = nullptr;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            dyn = reinterpret_cast<const Elf64_Dyn*>(base + phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return 0;

    const char*     strtab  = nullptr;
    const Elf64_Sym* symtab = nullptr;
    uint64_t         strsz  = 0;
    uint64_t         syment = sizeof(Elf64_Sym);

    for (const Elf64_Dyn* d = dyn; d->d_tag != DT_NULL; ++d) {
        if      (d->d_tag == DT_STRTAB)  strtab  = reinterpret_cast<const char*>(base + d->d_un.d_ptr);
        else if (d->d_tag == DT_SYMTAB)  symtab  = reinterpret_cast<const Elf64_Sym*>(base + d->d_un.d_ptr);
        else if (d->d_tag == DT_STRSZ)   strsz   = d->d_un.d_val;
        else if (d->d_tag == DT_SYMENT)  syment  = d->d_un.d_val;
    }

    if (!strtab || !symtab) return 0;

    // Iterate symbols — symtab ends where strtab begins (common layout).
    // We use strtab address as an upper bound.
    const uint8_t* sym_ptr = reinterpret_cast<const uint8_t*>(symtab);
    const uint8_t* sym_end = reinterpret_cast<const uint8_t*>(strtab);

    for (; sym_ptr < sym_end; sym_ptr += syment) {
        const auto* sym = reinterpret_cast<const Elf64_Sym*>(sym_ptr);
        if (sym->st_name == 0) continue;
        const char* sname = strtab + sym->st_name;
        if (str_cmp(sname, sym_name) == 0 && sym->st_value != 0) {
            return base + sym->st_value;
        }
    }
    return 0;
}
