#ifndef __KERN_MODULELOADER_MOD_H__
#define  __KERN_MODULELOADER_MOD_H__

#include <mod.h>
#include <elf.h>

/* Allocator used for allocating struct module, core sections and init 
   sections.  Returns NULL on failure. */
void *module_alloc(unsigned long size);
 
/* Free memory returned from module_alloc. */
void module_free(struct module *mod, void *module_region);

/* Apply the given relocation to the (simplified) ELF.	Return -error
	or 0. */
int apply_relocate(struct secthdr *sechdrs,
  					  const char *strtab,
  					  unsigned int symindex,
  					  unsigned int relsec,
  					  struct module *mod);
   
/* Apply the given add relocation to the (simplified) ELF.  Return
	-error or 0 */
int apply_relocate_add(struct secthdr *sechdrs,
  						  const char *strtab,
    					  unsigned int symindex,
  						  unsigned int relsec,
  						  struct module *mod);

/* Any final processing of module before access.  Return -error or 0. */
int module_finalize(const struct elfhdr *hdr,
					const struct secthdr *sechdrs,
					struct module *mod);

/* Any cleanup needed when module leaves. */
void module_arch_cleanup(struct module *mod);



#endif


