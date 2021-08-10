#### 3.lab3

###### 3.1 进程环境建立

第一部分的主要工作是将用户态执行的程序装载到内存中。为了与内核态区别开来，我们首先需要一种数据结构来管理用户态进程的相关环境信息，也就是Env结构体。在本部分中，我们初始化了一个env_free_list用于存储多个用户进程的环境信息，但在本实验中一次只会装载并执行一个用户程序。

Env映射的物理页由page_alloc分配，但这一步分配是在初始化env_free_list时就完成了的。真正要分配Env时内核所做的工作其实只是对env_free_list指向的Env进行初始化，并将其解下。

初始化时需要注意将内核中已建立的二级页表也拷贝给进程的二级页表，否则在从内核态转移至用户态时会发生page fault，但此时还未设置好异常处理，所以最终引发的是triple fault。这是因为在更改cr3为用户进程的二级页表后，还需要继续执行内核中的一系列pop指令并设置eip为用户程序的入口才能真正地转换到用户态，若用户进程页表中没有内核虚拟地址的相关映射关系，自然会发生page fault。初始化完二级页表后，还要将Env中各段寄存器的值设置成用户态地址空间的段选择子，最后从env_free_list上解下完成分配。

准备好了描述进程的Env，我们可以着手将硬盘上的二进制文件装载到内存中，这一步需要解析elf文件格式，分段映射到不同的物理页上。在映射时需要注意，我们存储映射关系的页表是用户进程所有的，而非内核的，因此我们需要更改cr3使当前生效的二级页表变为用户进程的，在映射完毕后再更改回内核的。映射完elf文件，还需要继续分配内存给用户栈，这样用户进程的内存布局才算完整了。最后将Env中用于上下文切换的Trapframe中的eip设置为程序入口点，这样内核才能正确地移交控制权。

###### 3.2 异常与中断处理

在上一部分中，我们实现了从内核态到用户态的转换。这一部分，我们要做的则是从用户态转换到内核态，相比于高权限到低权限，由低权限到高权限无疑要更加复杂。在操作系统中，这种转换只能通过中断完成，中断又分为外部中断与内部中断，外部中断主要是操作系统内核与外部设备通信的一种方式，在本实验中不会遇到；内部中断可进一步细分为软中断和异常，软中断是软件自己主动发起的中断，如int3、int80等，异常则是CPU内部执行时遇到错误发出的中断。我们接下来要实现的是内部中断的处理，由于软中断与异常的处理基本一致，我们接下来不对二者进行区分。

中断处理的基础数据结构是IDT(中断描述符表)，其中存储的描述符称为门。门描述符可分为中断门描述符，陷阱门描述符，任务门描述符和调用门描述符，后两者在实际应用中比较少见，因此可以忽略。前两个的区别主要在于中断门会将eflags寄存器IF位置0来屏蔽其他中断避免中断嵌套，而后者不会置0，其余的构成二者是一致的，都有段描述符选择子，段内偏移量和门描述符权限等。

因此我们在初始化idt时就要根据上述门描述符的结构来为每种中断设置好门描述符权限，和中断处理程序的偏移，段描述符选择子则统一为内核的代码段。中断处理程序的函数调用流程为trapEntry->trap->trap_dispatch，真正根据中断号执行不同异常处理的函数为trap_dispatch。trapEntry负责传递中断号，并根据不同中断是否带有error code来决定是否需要自行调整来保证各中断的栈帧结构相同，最后是保存用户态上下文环境。注意这个trapEntry对于每个中断来说是不同的，为了方便统一称为trapEntry。以下是各中断是否带有error code的情况。

![image-20210728165240665](https://i.loli.net/2021/07/28/ZUEBvwYOe5tQk6x.png)

trap则负责检查中断是否来自用户态，从而决定执行完中断处理后如何返回。

###### 3.3 系统调用

系统调用也是通过软中断实现的，因此处理方式与第二部分相同，我们实现系统调用只需要在idt中多加一项代表系统调用的门描述符即可，向其中存入系统调用trapEntry的地址。在trap_dispatch中提取用户通过寄存器传递的参数，然后将这些参数传递给真正根据系统调用号来提供不同功能的函数syscall。在执行不同系统调用对应得函数时，需要注意检查用户传递的参数，若为内存则必须进行权限的判断，以防止用户越权读写内存导致不可预料的后果。用户是否具有权限可通过页表中对应索引处存储的物理地址后12位来判断。这个判断还有另一个好处，若用户请求访问的内存是未映射的，那么就肯定不具有权限，从根本上保证了用户态的page fault不会导致内核态中发生page fault，换言之内核态中一旦发生page fault一定是不可修复的，系统应当直接panic。

###### 3.4 对问题的回答

- What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)

  不同的handler可以传递不同的中断号，若所有中断都由同一个trapEntry进入到内核态则内核中无法确定是哪一种中断。此外由于不同中断是否带有error code的情况也不一样，若是都使用一个trapEntry，传递给内核的Trapframe长度随中断不同也可能会不同。

- Did you have to do anything to make the `user/softint` program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but `softint`'s code says `int $14`. *Why* should this produce interrupt vector 13? What happens if the kernel actually allows `softint`'s `int $14` instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

  因为门描述符的权限控制，由用户态进程主动引发的中断权限只有3，而page fault的门描述符权限要求为0，不符合权限就转变成了general protection fault。若内核允许用户态主动引发page fault，那么用户可以间接地控制物理页的分配。

- The break point test case will either generate a break point exception or a general protection fault depending on how you initialized the break point entry in the IDT (i.e., your call to `SETGATE` from `trap_init`). Why? How do you need to set it up in order to get the breakpoint exception to work as specified above and what incorrect setup would cause it to trigger a general protection fault?

  这个问题的原因也是门描述的权限设置不正确导致的，int3是用户态进程主动引发的，权限只有3，若int3的门描述符权限设置为比3小的数就会变成general protection fault。因此将门描述符权限设置为3就能够正常的触发int3中断处理。

- What do you think is the point of these mechanisms, particularly in light of what the `user/softint` test program does?

  门描述符的权限设置至关重要，若是权限设置错误轻则无法正常处理中断，重则导致用户越权。

- 编译相关问题

  发现使用gcc-8编译会导致所有测试都过不去，问题出在kern_pgdir初始化上，memset清空kern_pgdir后，发现kern_pgdir变量本身也变为了0。ida看了一下kernel发现end居然在kern_pgdir前，不知道为啥。只能把在end后的变量都赋一个初始值，免得被放到bss段。

  ![image-20210809113012033](https://i.loli.net/2021/08/09/gTE9PWpdS57qMVz.png)