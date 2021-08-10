#### 6.lab6

###### 6.1 总体结构

本实验主要组成部分如实验指导中的图所示

![ns](https://i.loli.net/2021/08/09/8LCv5AHj7GeahtQ.png)

E1000网卡对应的是数据链路层，Core network中的lwIP对应的是网络层与传输层，httpd对应的则是应用层。E1000 driver的实现是本实验的主要工作内容，它负责从E1000接收数据包或是发送数据包到E1000，此外几个helper也是我们需要实现的，它们使用ipc实现对network server数据包的收发，这些helper通过系统调用的方式与我们写的driver通信。这样一套完整协议模型主要是为了方便检验E1000 driver的实现正确与否，但我们对于网络协议的实现过程并不关心，所以直接使用了lwIP，这是一个轻量级的TCP/IP协议栈。

###### 6.2 E1000 driver

E1000的manual：https://pdos.csail.mit.edu/6.828/2018/readings/hardware/8254x_GBe_SDM.pdf

- init

  E1000网卡是PCI设备，在系统启动时会沿着PCI总线寻找存在的PCI设备，找到了就会调用事先注册了的初始化函数。这些初始化函数存储在pci_attach_vendor数组中，通过设备的vendor ID和device ID来确定调用哪个函数。这个数组单个元素构成如下所示。

  ![image-20210809153245907](https://i.loli.net/2021/08/09/mbiH69FVYEjRrO2.png)

  初始化函数共完成4件工作：

  - 启用设备为其分配资源

    直接调用pci_func_enable函数。

  - 映射设备寄存器到内存空间

    调用mmio_map_region从为mmio留出的虚拟内存空间中割一块下来。

  - transmit相关寄存器初始化设置

    参照manual 14.5节。

  - receive相关寄存器初始化设置

    参照manual 14.4节。不过manual对于RDH和RDT初始化描述的不太准确，这两个寄存器其实存的并不是队列头尾地址，而是队列的索引，RDH为0，RDT为索引最大值。

- DMA

  E1000 driver的作用是更高效地传递数据包，试想一下没有它的话，我们就只能通过直接读写E1000的寄存器来传输数据了，这无疑是低效率的。有了这个driver之后，它会维护两个循环队列，一个存储待transmit的数据包，另一个存储待receive的数据包，队列中的元素是描述符，除了包含数据包的内容还存有该数据包状态以及控制字段。这样的话只需要将这两个队列的指针和队列大小传递给E1000寄存器，E1000就能够读写driver内存中的数据包，事实上这正是init中设置寄存器中的一部分。循环队列使用头指针与尾指针来描述，在头指针与尾指针之间的就是可用的数据包，driver通过调节这2个指针来通知E1000数据包的处理情况。注意所有指针都需要使用物理地址，因为E1000是直接读写物理RAM的，不会经过MMU进行地址转换。以上就是DMA(直接内存存取)的描述。

  - transmit

    这个操作的方向是network server->E1000，也就是向外发送数据包。队列头指针指向的描述符是当前正被发送的数据包，尾指针指向的描述符是待添加的数据包。需要注意的是，尾指针指向的描述符包含的数据包不一定是空闲的，也有可能正被E1000处理，说明此时待发送的数据包已满，所以我们需要判断状态中的DD位是否为1，若为1才说明该数据包已处理完毕，若不为1的话driver就需要返回错误信息。在添加数据包到队列时，需要设置控制字段中的RS和EOP，前者是为了E1000在处理完数据包后自动设置DD位，后者标识数据包分片的结尾，因为本实验中不会出现大到需要分片的数据包，所以每次发送都可以设置。

  - receive

    这个操作的方向是E1000->network server，也就是接收外来数据包。队列头指针指向的描述符是当前收到的数据包，尾指针指向的描述符包含的是刚接收过的数据包。因此receive中先将尾指针自增，再判断描述符状态中DD位是否为1，若不为1则说明当前队列是空的，需要返回错误信息。

###### 6.3 output/input helper

output比较简单，循环地调用ipc_recv接收network server发来的数据包，然后使用系统调用传给E1000 driver即可。

input需要维护一个队列，队列中每个元素大小为1页，取当前元素作为ipc_send的参数。这么做的原因是当input调用完ipc_send后就会继续去接收E1000传来的下一个包，然后立刻发送给network server，若是仍像output一样使用nsipcbuf来存储参数，那么可能network server还没处理完上一个包的内容，数据就已经被这个包给覆盖了。

###### 6.4 question

- How did you structure your transmit implementation? In particular, what do you do if the transmit ring is full?

  队列未满时直接设置好RS与EOP，然后将数据包加入队列，再自增TDT。队列满了就返回失败信息。

- How did you structure your receive implementation? In particular, what do you do if the receive queue is empty and a user environment requests the next incoming packet?

  先自增RDT，再判断RDT对应描述符DD位，若为1则说明有包，拷贝到buf中并设置好size参数返回。队列为空则返回失败信息。因此input helper中需要循环调用recv包的系统调用直到接收到数据包。

- What does the web page served by JOS's web server say?

  This file came from JOS.

  Cheesy web page!

- How long approximately did it take you to do this lab?

  花费了大约3天，有1天时间浪费在实现receive中断上，但发现即使按manual上说的设置好了IMS和RDTR寄存器也不会产生中断，遂放弃。卡住我的另一个地方是RDH与RDT的设置上，按manual说的设置总是无法正常收到包。总的来说大多数时间是因manual不准确而浪费的。