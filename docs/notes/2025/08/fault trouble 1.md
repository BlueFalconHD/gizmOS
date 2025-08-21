# Sunday 17th

I am having lots of trouble with faults. After developing a new tool to help read page tables in LLDB and debug what is going wromg, I determined only the first 10KiB of code are being mapped as executable.

(lldb) mmu-walk 0xffffffff8000faaa
SATP=0x80000000000fad18 mode=SV39 levels=3 asid=0x0 root_ppn=0xfad18 hhdm=0xffffffc000000000
root table: PA=0x00000000fad18000 VA=0xffffffc0fad18000
L2 idx=510 PTE@PA=0x00000000fad18ff0 VA=0xffffffc0fad18ff0 PTE=0x000000003eb45c01 [V]
L1 idx=  0 PTE@PA=0x00000000fad17000 VA=0xffffffc0fad17000 PTE=0x000000003eb45801 [V]
L0 idx= 15 PTE@PA=0x00000000fad16078 VA=0xffffffc0fad16078 PTE=0x000000003ead7ccb [VRXAD] LEAF
RESOLVED: VA=0xffffffff8000faaa -> PA=0x00000000fab5faaa page_size=4 KiB (level 0)
(lldb) mmu-walk 0xffffffff80014960
SATP=0x80000000000fad18 mode=SV39 levels=3 asid=0x0 root_ppn=0xfad18 hhdm=0xffffffc000000000
root table: PA=0x00000000fad18000 VA=0xffffffc0fad18000
L2 idx=510 PTE@PA=0x00000000fad18ff0 VA=0xffffffc0fad18ff0 PTE=0x000000003eb45c01 [V]
L1 idx=  0 PTE@PA=0x00000000fad17000 VA=0xffffffc0fad17000 PTE=0x000000003eb45801 [V]
L0 idx= 20 PTE@PA=0x00000000fad160a0 VA=0xffffffc0fad160a0 PTE=0x000000003ead90c7 [VRWAD] LEAF
RESOLVED: VA=0xffffffff80014960 -> PA=0x00000000fab64960 page_size=4 KiB (level 0)
(lldb) image lookup -n VA=0xffffffff8000faaa
(lldb) image lookup -a 0xffffffff8000faaa
      Address: kernel[0xffffffff8000faaa] (kernel.PT_LOAD[1]..text + 60074)
      Summary: kernel`gizm_font_draw_text at gizm_font.c:268
(lldb) image lookup -a 0xffffffff80014960
      Address: kernel[0xffffffff80014960] (kernel.PT_LOAD[1]..text + 80224)
      Summary: kernel`trap_vector
(lldb)

Currently I am working on a fix which uses a linker section eqiuivalent to text called text-early where I will put all early-boot code that sets up my page tables, which correctly maps the entire kernel as executable.

For organization purposes, I have added a new .h/.c pair called earlyinit where all functions accessed from within must be marked as earlyinit.

Early init functions must use the EARLY_TEXT macro to place them in the text-early section, which is mapped as executable.
