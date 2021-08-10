#### 4.lab4

###### 4.1 多处理器支持与多任务实现

- 映射mmio的虚拟内存

  mmio是一种与设备通信的方式，它将虚拟内存地址映射到设备的物理内存空间，在这些虚拟内存上读写即可实现与外部设备的通信。因此我们需要空出一些虚拟内存空间留作此用。

- lapic初始化

  由于我们内核被装载的物理地址较低，所以我们之前一直是通过+0xf0000000的虚拟内存地址映射到内核的物理地址。但lapic所在物理地址从0xFE000000开始，我们不能使用这种方式了，此时上一步留出的虚拟内存空间就可以在这步用上了。

  接下来的工作是设置lapic实现定时产生时钟中断，这部分设置主要是通过mmio与硬件设备通信实现，下文中CPU的设置也类似，只是通信方式还有pmio，由于涉及到许多与本实验无关的CPU内部细节，不具体分析了。

- BSP启动APs

  该部分完成的工作是由自举CPU(BSP)拉起其他的CPU(AP)，在本实验中BSP固定为第一个CPU。它会将启动用的汇编指令(kern/mpentry.S)拷贝到物理地址0x7000处，设置好mpentry.S所需的栈地址（这块内存在mem_init中完成映射），然后与对应AP通信使其运行该部分，然后BSP就会阻塞直到该AP启动完毕。mpentry.S使CPU进入保护模式并启动分页功能，最后调用了mp_main。这里有一个问题：为何mp_main的调用需要使用间接调用。这是因为mp_main是以绝对地址形式传入的，编译器无法计算它与下一条指令的偏移，因此无法直接调用。

  mp_main中为该AP初始化lapic，加载GDT和段描述符，初始化并在GDT中设置TSS。随后将状态设置为CPU_STARTED，告知BSP启动已完毕。最后调用sched_yield任务调度函数，寻找一个待执行的任务给该AP执行。

- 加锁

  经过上一步，我们此时已经有多个AP在同时执行任务了，它们随时可能由用户态转换至内核态，因此必须要考虑避免条件竞争问题，尤其是在内核使用了大量全局变量的情况。所以要在trap函数中以及所有sched_yield前加锁，在env_run最后进入用户态之前释放锁即可。

  两种加锁的位置可以抽象成进程处于内核态的时机，一个进程除了在启动时处于内核态，就只有可能通过中断进入内核态，前者对应i386_init与mp_main中加锁，后者对应在trap中加锁。

- 任务调度算法

  sched_yield采用的调度算法是轮询算法(Round-Robin)，这个算法的原理是从上一次位置的后一个开始，循环地查询到上回的位置处，这个过程中将遇到的第一个RUNNABLE任务作为本次执行的任务。若是没有找到任何RUNNABLE的任务，而且上回的任务仍然处于RUNNING状态，就继续执行上回的任务。

###### 4.2 Copy-on-Write的fork实现

- 多进程相关系统调用

  在内核中需要实现一些多进程编程中可能会用到的系统调用，以方便用户调用。以下涉及到的所有进程均为当前进程或其子进程，涉及到的虚拟地址均为页对齐并且小于UTOP，涉及的权限设置中PTE_U 与PTE_P为必选，PTE_AVAIL与PTE_W为可选。不满足以上任意条件，我们的系统调用都会失败。
  
  - sys_exofork
  
    分配出一个env，并把当前env作为其父进程，随后将父进程的trapframe拷贝给它，只是eax设置成0。等之后CPU运行这个进程返回用户态时，会返回到和父进程相同的位置，只是返回值变为了0。
  
  - sys_env_set_status
  
    允许用户更改某个进程的状态为ENV_NOT_RUNNABLE或ENV_RUNNABLE。
  
  - sys_page_alloc
  
    分配一个物理页，并将用户指定的虚拟内存地址映射到其上。
  
  - sys_page_map
  
    将某进程的某个虚拟内存地址映射到某进程的某个虚拟内存地址所映射的物理页。
  
  - sys_page_unmap
  
    解除某进程中一处虚拟内存地址与物理页的映射关系。

- Copy-on-Write的意义

  xv6中实现fork的方式是通过sys_exofork分配出子进程的env，然后调用sys_page_alloc与sys_page_map将子进程的虚拟内存都映射到物理页上，并将父进程的内存空间完整地拷贝到子进程的内存空间。这一步的开销是巨大的，每次fork都要消耗掉当前进程等量的物理页，然而子进程对这些内存空间可能并不会进行什么操作，例如shell中fork出的子进程主要负责调用execve系统调用执行命令，只是设置一下寄存器就进入内核态了，为子进程分配的物理页并没有派上实际用场。因此为了减少开销，对于fork出来的子进程我们让其与父进程共用物理页，只有当二者中有需要写入内存空间造成二者内存空间不一致时，我们再单独分配额外的物理页。

- 用户态缺页中断处理

  在父进程fork出子进程后，它们映射的物理页权限都变为了COW，这意味着写入时会触发缺页中断。我们需要在page_fault_handler函数中实现用户态缺页中断的处理方法——之前我们只实现了内核态的处理。

  用户态缺页中断处理的大体思路是将trapframe压入异常处理栈中，然后跳转到用户注册的异常处理函数（此时已经返回到用户态），这个函数负责调用真正处理异常的函数\_pgfault_handler，处理完毕后无法回到内核态，必须直接在用户态下恢复原状态，因此要手动实现寄存器恢复与栈帧切换并返回发生缺页中断的那条指令，本实验中该函数为_pgfault_upcall。

  缺页中断的情况可分为2种，一种是在缺页中断处理函数中又发生了缺页中断导致的嵌套中断，另一种是单纯的非嵌套中断。二者的区别在于发生异常时所使用的栈空间不同，前者使用的是异常处理栈，位于UTOP；后者使用的是用户栈。

  - 嵌套中断

    在嵌套中断中，我们在压入trapframe之前还要多压入4字节的空白数据，这是因为_pgfault_upcall恢复所有寄存器的同时还要返回发生异常的地址，它必须要在发生异常时使用的栈上压入返回地址，这样才能在恢复esp之后正确地恢复eip。如果我们不多加这4字节，由于嵌套中断中异常处理栈和发生异常时的栈实际上连在一起的，那么在压入返回地址时就会覆盖异常处理栈帧中的trapframe最高的4字节即esp。

    ![pgfault_handle_ret](https://i.loli.net/2021/08/02/hfKTayWSLCN59pX.png)

  - 非嵌套中断

    非嵌套中断中我们不需要多压这4字节，只需要将栈帧转移到异常处理栈中，然后再压入trapframe。因为发生异常时的esp-4处肯定是无用的数据。

- fork实现细节

  以下是与dumbfork实现的不同之处。

  - 调用sys_pgfault_handler为父进程设置_pgfault_upcall与\_pgfault_handler

  - duppage

    先调用sys_page_map将父子进程的同一虚拟内存地址映射到同一物理页，并且此时子进程对应虚拟内存权限为COW。接下来将调用sys_page_map将父进程自身重新映射一遍，这步是为了修改父进程虚拟内存权限为COW。

  - pgfault

    这是fork中被注册成\_pgfault_handler的函数。大致工作流程为：将PFTEMP映射到一个新的物理页；我们把缺页中断所在页地址记作fault_va，然后将fault_va那一页数据拷贝到PFTEMP处；最后调用sys_page_map将fault_va映射到本进程PFTEMP对应的物理页，并设置权限为可写。由此可以看出做的工作是利用PFTEMP作为暂存数据的内存空间，实现了fault_va映射物理页的替换。

  - 设置子进程_pgfault_upcall，由于子进程与父进程共用内存空间，因此不需要再设置\_pgfault_handler

###### 4.3 进程间通信

- 时钟中断

  在第一部分的初始化工作中我们提到了用lapic来定时产生时钟中断，接下来这部分就要利用lapic产生的时钟中断实现CPU的时分复用了。

  实现思路是为用户态进程的eflags寄存器设置FL_IF标志代表启用时钟中断，并在trapentry和trap_dispatch函数中添加对时钟中断的处理。具体来说就是当用户态进程接收到时钟中断时，会陷入内核把当前正处于RUNNING状态的任务改为RUNNABLE，然后调用lapic_eoi和sched_yield调度另一个合适的任务来执行。这样保证了每个任务都是按照分到的时间片来执行，没有哪个任务能够一直霸占CPU，时间片大小就是lapic产生时钟中断的间隔。最后要注意的一点，lapic_eoi的功能是向lapic发送接收到时钟中断的响应，因为x86架构CPU只有接收到响应才会产生下一个时钟中断。

- 进程间信息的发送与接收

  - 接收信息

    进程通过系统调用sys_ipc_recv进入内核态，将进程对应的env中的env_ipc_receving标志位设为真表示处于接收信息状态，随后将自身进程状态改为ENV_NOT_RUNNABLE并调度另一个新任务。只有等到某个进程向它发送数据并将其状态改回ENV_RUNNABLE，它才有可能被CPU执行。

  - 发送信息

    这部分系统调用比较繁琐，大多是一些差错检查，若是发送方与接收方的虚拟内存地址都合法，则发送方虚拟内存地址对应的物理页会与接收方的虚拟内存地址建立映射关系，与sys_page_map的实现类似，否则就不映射物理页。无论是否映射物理页，最终都要把接收者的env_ipc_receving设置为假，env_ipc_from设置为自身的进程id，env_ipc_value设置成要发送的数据，env_status设置为ENV_RUNNABLE，且最重要的是要将接收方进程trapframe中的eax寄存器设为0来模拟正常返回用户态的情况。

  - 一个小坑

    在实现IPC时我发现即使设置了trapframe的eax为0，但用户态得到的返回值却为12，也就是sys_ipc_recv的调用号。一度困扰了我很久，最后发现问题出在env_run函数中，我们的sys_ipc_recv在设置了自身为NOT_RUNNABLE之后，又会因为env_run切换进程时设置回RUNNABLE。导致还没等到进程发送信息给它，它自身又运行起来最终没收到任何信息地返回了用户态，当然eax寄存器也是保持着调用时的原样为12。至此我明白了lab3中对env_run实现的提示，原来指的是还需要考虑当前进程为NOT_RUNNABLE的状态。

###### 4.4 question

- Compare `kern/mpentry.S` side by side with `boot/boot.S`. Bearing in mind that `kern/mpentry.S` is compiled and linked to run above `KERNBASE` just like everything else in the kernel, what is the purpose of macro `MPBOOTPHYS`? Why is it necessary in `kern/mpentry.S` but not in `boot/boot.S`? In other words, what could go wrong if it were omitted in `kern/mpentry.S`?

  Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

  因为mpentry.S中的指令及数据会被加载到MPENTRY_PADDR处再执行，因此相关数据的绝对地址要修改成加载后的地址，只需要将这些数据的偏移量加上MPENTRY_PADDR即可。如果没有MPBOOTPHYS，而是直接用它们在mpentry.S中的地址会导致访问不可用内存的错误。boot.S的加载地址与编译链接后的地址相同，所以不需要计算偏移与加载基址相加。

- It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

  因为这把锁在最后调用env_pop_tf回到用户态前必须要解开，这意味着可能会有多个CPU同时执行env_pop_tf。如果所有CPU使用同一个栈空间，那么env_pop_tf所使用的参数会遭到破坏。

- In your implementation of `env_run()` you should have called `lcr3()`. Before and after the call to `lcr3()`, your code makes references (at least it should) to the variable `e`, the argument to `env_run`. Upon loading the `%cr3` register, the addressing context used by the MMU is instantly changed. But a virtual address (namely `e`) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer `e` be dereferenced both before and after the addressing switch?

  因为envs是在mem_init中由boot_alloc分配出来的，位于内核的虚拟内存空间，内核虚拟地址与物理地址的转换方式固定为+-0xf0000000，这对于任何进程的页表来说都是如此，换言之任何页表0xf0000000之后的虚拟内存空间映射的都是相同的物理页，所以e的使用不受切换页表的影响。

- Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

  因为转换到新的环境之后，新的进程会按照指令修改寄存器中的值，如果不保存转换之前寄存器的值，那么这些数据就永远地丢失了。保存寄存器值到trapframe中的操作在trapentry.S的_alltraps中完成，这是所有类型的中断都会执行的一段指令。