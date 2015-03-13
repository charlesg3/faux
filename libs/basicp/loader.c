#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libelf.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdbool.h>

// Taken from here:
//http://stackoverflow.com/questions/13908276/loading-elf-file-in-c-in-user-space

static int is_image_valid(Elf32_Ehdr *hdr)
{
    if(hdr->e_type != 3)
    {
        fprintf(stderr, "ERROR: Cannot load non-PIE programs.\n");
        return 0;
    }
    return 1;
}

static void *resolve(const char* sym)
{
    static void *libc_handle = NULL;
    if (libc_handle == NULL) libc_handle = dlopen("libc.so", RTLD_NOW);
    return dlsym(libc_handle, sym);
}

static void relocate(Elf32_Shdr* shdr, const Elf32_Sym* syms, const char* strings, const char* src, char* dst)
{
    Elf32_Rel* rel = (Elf32_Rel*)(src + shdr->sh_offset);
    int j;
    for(j = 0; j < shdr->sh_size / sizeof(Elf32_Rel); j += 1) {
        const char* sym = strings + syms[ELF32_R_SYM(rel[j].r_info)].st_name;
        switch(ELF32_R_TYPE(rel[j].r_info)) {
            case R_386_JMP_SLOT:
            case R_386_GLOB_DAT:
                *(Elf32_Word*)(dst + rel[j].r_offset) = (Elf32_Word)resolve(sym);
                break;
        }
    }
}

static void* find_sym(const char* name, Elf32_Shdr* shdr, const char* strings, const char* src, char* dst)
{
    Elf32_Sym* syms = (Elf32_Sym*)(src + shdr->sh_offset);
    int i;
    for(i = 0; i < shdr->sh_size / sizeof(Elf32_Sym); i += 1)
        if (strcmp(name, strings + syms[i].st_name) == 0)
            return dst + syms[i].st_value;

    return NULL;
}

void *image_load (char *elf_start, unsigned int size)
{
    Elf32_Ehdr      *hdr     = NULL;
    Elf32_Phdr      *phdr    = NULL;
    Elf32_Shdr      *shdr    = NULL;
    Elf32_Sym       *syms    = NULL;
    char            *strings = NULL;
    char            *start   = NULL;
    char            *taddr   = NULL;
    void            *entry   = NULL;
    int i = 0;
    char *exec = NULL;

    hdr = (Elf32_Ehdr *) elf_start;

    if(!is_image_valid(hdr))
        exit(-1);

    exec = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    assert(exec);

    // Start with clean memory.
    memset(exec,0x0,size);

    phdr = (Elf32_Phdr *)(elf_start + hdr->e_phoff);

    for(i=0; i < hdr->e_phnum; ++i) {
        if(phdr[i].p_type != PT_LOAD) continue;

        if(phdr[i].p_filesz > phdr[i].p_memsz) {
            munmap(exec, size);
            assert(false);
            return 0;
        }

        if(!phdr[i].p_filesz) continue;

        // p_filesz can be smaller than p_memsz,
        // the difference is zeroe'd out.
        start = elf_start + phdr[i].p_offset;
        taddr = phdr[i].p_vaddr + exec;

        memmove(taddr,start,phdr[i].p_filesz);

        // Read-only.
        if(!(phdr[i].p_flags & PF_W))
            mprotect((unsigned char *) taddr, phdr[i].p_memsz, PROT_READ);

        // Executable.
        if(phdr[i].p_flags & PF_X)
            mprotect((unsigned char *) taddr, phdr[i].p_memsz, PROT_EXEC);
    }

    shdr = (Elf32_Shdr *)(elf_start + hdr->e_shoff);

    for(i=0; i < hdr->e_shnum; ++i)
        if (shdr[i].sh_type == SHT_DYNSYM) {
            syms = (Elf32_Sym*)(elf_start + shdr[i].sh_offset);
            strings = elf_start + shdr[shdr[i].sh_link].sh_offset;
            entry = find_sym("main", shdr + i, strings, elf_start, exec);
            break;
        }

    for(i=0; i < hdr->e_shnum; ++i)
        if (shdr[i].sh_type == SHT_REL)
            relocate(shdr + i, syms, strings, elf_start, exec);

   return entry;

}/* image_load */

