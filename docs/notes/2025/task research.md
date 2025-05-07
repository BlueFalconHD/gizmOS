# April 24, 2025 - Research on how xv6 (specifically RISC-V) handles multitasking/tasks

As of right now, my kernel is only in supervisor mode, and I can't run anything else without blocking the kernel. To ensure that development is sustainable and that I can avoid porting any features implemented in the kernel to user mode, I have decided to implement multitasking and a user mode. This will allow me to run multiple tasks concurrently and avoid blocking the kernel. I will be using the [xv6 kernel](https://github.com/mit-pdos/xv6-riscv/) as a reference.

xv6 is an UNIX-like operating system designed as a tool to educate students about operating systems. It is very well documented, which makes it a great reference.

## Multitasking in xv6

- main -> kvmmake:
  - map kernel and devices to virtual addresses
  - map trampoline
  - kvmmake -> proc_mapstacks:
    - for each process in process table (uninitialized at this point):
      - allocate a physical page for the stack (used only for kernel handling of the process like handling syscalls/traps, etc, not used in userspace0)
        - calculates the virtual address using the KSTACK macro
      - doesn't populate the proc data structure

- main -> procinit:
  - for each process in the process table:
    - initializes the lock
    - sets the state to unused
    - sets the kstack to the virtual address of the stack (calculated using the KSTACK macro)

- main -> userinit:
  - userinit -> allocproc:
    - finds empty process in the process table
    - allocates a trapframe for the process and sets its address
    - allocproc -> proc_pagetable:
      - proc_pagetable -> uvmcreate:
        - allocates a page table and zeroes it out
      - maps highest virtual address to physical address of trampoline in trampoline.S, in special section in .text (read and execute)
      - maps trapframe 1 page below trampoline, to address set previously
    - zero out the context
    - sets the process context's RA to forkret
    - sets the process context's SP to the top of the kstack for kernel handling (p.kstack + PGSIZE)
  - userinit -> uvmfirst:
    - allocates a page for process initcode
    - allocates page for initcode
    - maps VA 0 to PA of allocated page
    - copies initcode to the allocated page
  - sets trapframe's EPC to 0 (the entry point of initcode)
  - sets trapframe's SP to 0x1000 (TODO: figure out why)
  - sets process name, cwd
  - sets process state to runnable

- main -> scheduler:
  - infinitely loops and searches for process with state runnable
    - enables interrupts
    - scheduler -> swtch with cpu context and process kernel context:
      - saves scheduler's register state with ra after call to swtch
        - registers saved/restored:
          - s0-s11
          - sp
          - ra
      - loads process's kernel context
      - jumps to ra of kernel context
        - if this is the first time the process is run, it will jump to forkret


- forkret:
  - inits filesystem once if it isn't already because it must be done in userspace
  - forket -> usertrapret:
    - disable interrupts
    - set stvec to uservec in process trampoline
    - set trapframe values used by uservec:
      - kernel_satp to kernel page table
      - kernel_sp to top of process's kstack
      - kernel_trap to address of usertrap
      - kernel_hartid to hartid
    - prepare sstatus for user mode
      - set SPP to 0 (user mode)
      - set SPIE to 1 (interrupts enabled for user mode)
    - set sepc to epc saved in trapframe
    - makes SATP value from p->pagetable
    - usertrapret -> userret with satp value:
      - set page table (satp)
      - restore every register except for a0 from trapframe:
        - ra, sp, gp, tp
        - t0-t6
        - s0-s11
        - a1-a7
      - restore a0 from a0 in trapframe
      - sret to user mode and pc

- uservec:
  - save a0 to sscratch and load address of TRAPFRAME
  - save all registers to trapframe except for a0
    - ra, sp, gp, tp
    - t0-t6
    - s0-s11
    - a1-a7
  - restore a0 from sscratch and save it to trapframe
  - load kernel_sp, kernel_hartid into sp and tp
  - fetch address of usertrap from kernel_trap and page table value from kernel_satp
  - install page table
  - uservec -> usertrap:
    - make sure SSTATUS_SPP is set to user mode
    - set stvec to kernel trap handler
    - get current process
    - save current sepc to trapframe.epc
    - if scause is 8 (syscall):
      - check if process is killed
        - usertrap -> exit:
          - check if process is init process, if so panic
          - do stuff with filesystem, close open files etc.
          - exit -> reparent with myproc:
            - for all processes in table, if any have input process as parent, set their parent to init process
            - reparent -> wakeup with input process:
              - loop through all processes except for the input process
                - if their state is sleeping and their chan is equal to the input process, set their state to runnable
          - wakeup parent of input process
          - set proc.xstate to proc.state
          - set proc.state to ZOMBIE
          - call sched
          - panic
      - increment trapframe.epc by 4 to skip the syscall instruction
      - enable interrupts
    - else if (which_dev = usertrap -> devintr) != 0
      - we're okay, interrupt is handled
    - else:
      - log unexpected scause and kill process
    - check if process is killed
      - usertrap -> exit:
        - TODO: describe
    - if which_dev is 2:
      - devintr -> yield:
        - get myproc
        - set p.state to runnable
        - yield -> sched:
          - check conditions and panic if not met:
            - mycpu.noff == 1
            - myproc.state == RUNNING
            - sstatus & SSTATUS_SIE == 0 (intr_get)
          - set temporary intena to mycpu.intena
          - sched -> swtch with myproc.context and mycpu.context
          - mycpu.intena = temporary intena
    - usertrap -> usertrapret


- devintr:
  - read scause
  - if it is external interrupt (0x9):
    - plic stuff
    - return 1
  - if it is a timer interrupt (0x5):
    - devintr -> clockintr:
      - if I am cpu 0:
        - increment ticks global
        - clockintr -> wakeup:
          - loop through processes, look for sleeping processes whose chan value is equal to current ticks:
            - set its state to runnable
      - set timecmp to csrr + 1000000
    - return 2
  - otherwise:
    - return 0
