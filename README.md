<h1>
    <div>操作系统第二次实验 - FAT12</div>
    <div>Operating System Lab 2 - FAT12</div>
</h1>

## 启动项目 Project setup

``` shell
./start.sh
```

## 清除生成的文件 Clean generated files

``` shell
./clean.sh
```



## 实验二问题清单

### 2.1 PPT相关内容

1. #### 什么是实模式，什么是保护模式？

    实模式就是用基址 + 偏移量就可以直接拿到物理地址的模式 。

    保护模式就是不能直接拿到物理地址的模式，需要进行地址转换。  

2. #### 什么是选择子？

   选择子共16位，放在段选择寄存器里

   低2位表示请求特权级RPL（Request Privilege Level ）

   第3位表示选择GDT方式还是LDT方式

   高13位表示在描述符表中的**偏移**（故描述符表的项数最多是2的13次方）

3. #### 什么是描述符？

   保护模式下引入描述符来描述各种数据段，所有描述符均为8字节，由第5个字节说明类型。类型不同，描述符结构也不同。一般由段界限、段基址1、属性、段界限2组成。

4. #### 什么是GDT，什么是LDT?

   ##### GDT

   全局描述符表，是全局唯一的。存放一些公用的描述符、和包含各进程局部描述符表首地址的描述符。

   ##### LDT

   局部描述符表，每个进程都可以有一个。存放本进程内使用的描述符。  

   两者关系类似于全局变量（全局的描述符）和局部变量（各进程的描述符），但实际上形成了一个二级的表结构，GDT里包含LDT的入口。

5. #### 请分别说明GDTR和LDTR的结构。

   ##### GDTR

   48位寄存器，高32位放置GDT首地址，低16位放置GDT限长（限长决定了可寻址的大小，注意低16位放的**不是选择子**）

   > 真的很像页表基址寄存器。

   ##### LDTR

   16位寄存器，放置一个特殊的选择子，用于查找当前进程的LDT首地址。

   > 有点像地址解析协议ARP。当然并没有什么关系，只是一种无端的联想。

6. #### 请说明GDT直接查找物理地址的具体步骤。

   > 其实有点像页表。段选择子像是虚拟地址，分为页号 + 偏移量，描述符像是物理地址，分为页框 + 偏移量。GDT就是页表。

   - 给出段选择子（放在段选择寄存器里）+ 偏移量

   - 若选择了GDT方式，则从GDTR获取GDT首地址，用段选择子中的13位做偏移，拿到GDT中的描述符

   - 如果**合法且有权限**，用描述符中的段首地址加上（1）中的偏移量找到物理地址。寻址结束。

7. #### 请说明通过LDT查找物理地址的具体步骤。

   > 有点像多级页表。从GDT这个大页表里找到LDT这个小页表，然后在小页表里进行查询。其实就是递归了一下。

   - 给出段选择子（放在段选择寄存器中）+ 偏移量
   - 若选择了LDT方式，则从GDTR获取GDT首地址，用LDTR中的偏移量做偏移，拿到GDT中的描述符1
   - 从描述符1中获取LDT首地址，用段选择子中的13位做偏移，拿到LDT中的描述符2
   - 如果**合法且有权限**，用描述符2中的段首地址加上（1）中的偏移量找到物理地址。寻址结束。

8. #### 根目录区大小一定么？扇区号是多少？为什么？

   不一定，需要计算，取决于BPB_RootEntCnt * BPB_BytsPerSec，最大根目录文件数 * 每扇区字节数。

   但是根目录区扇区号倒是固定的，开始于19扇区。因为引导扇区占1扇区，两张互为冗余FAT表各占9扇区，1 + 9 + 9 = 19，也就是开始于地址0x2600。

9. #### 数据区第一个簇号是多少？为什么？

   2。因为FAT表前两项已经是固定的0xFF0、0xFFF了，而FAT表和数据簇一一对应，前两个数据簇0、1就失去了意义，因为不会有指向它们的FAT项。所以从2开始。

10. #### FAT表的作用？

    用来指示每一个文件的下一个数据区的簇号，类似于链表的指针。

11. #### 解释静态链接的过程。

    在编译时直接把静态库加入到可执行文件。此时需要空间、地址分配，符号解析和重定位。

12. #### 解释动态链接的过程。

    链接阶段仅仅只加入一些描述信息，程序执行时再加载相应的动态库并计算对应的逻辑地址。

    在程序启动前，动态链接库中的变量处于bss段中，未初始化。

    动态链接的函数则会跳转到0x601030（.got.plt表）位置所指向的值的地址的函数处。

    ![3.png](https://i.loli.net/2019/11/09/eZ8mlBsUWTLuH62.png)

    **.got**

    Global Offset Table，全局偏移表。这是链接器在执行链接时实际上要填充的部分，保存了所有外部符号的地址信息。

    **.plt**

    Procedure Linkage Table，进程链接表。这个表里包含了一些代码，用来：

    - 第一次执行时，**调用链接器**来解析某个外部函数的地址，并填充到.got.plt中，然后跳转到该函数

    - 如果已经填充过，直接在.got.plt中查找并跳转到对应外部函数

    **.got.plt**

    .got.plt相当于.plt的GOT全局偏移表，其内容有两种情况：

    - 如果在之前查找过该符号，内容为外部函数的具体地址

    - 如果没查找过，则内容为跳转回.plt的代码，并执行查找

13. #### 静态链接相关PPT中为什么使用ld链接而不是gcc

     事实上，gcc调用的就是ld，但是使用gcc不用手动指定C标准库，而ld需要（因为ld更底层）。所以我不知道他为什么一定要用ld，可能是参数不一样吧。或者如果非要说，gcc是一整套操作，ld单独是链接。

     > 可能的一种解释：使用ld进行连接的原因是为了避免gcc 进行glibc 的链接。用gcc 的话有可能去调C库，使程序环境变得复杂，所以用ld）。

14. #### linux下可执行文件的虚拟地址空间默认从哪里开始分配。

    32位下，ELF可执行文件默认加载到地址0x08048000。ld的默认脚本用这个地址作为ELF的起始地址。实话说原因不明，似乎是一个Magic Number。

    64位下，**据说**是加载到0x40000000。原因同样不明。

    > On 386 systems, the text base address is 0x08048000, which permits a reasonably large stack below the text while still staying above address 0x08000000, permitting most programs to use a single second-level page table. (Recall that on the 386, each second-level table maps 0x00400000 addresses.)
    >
    > 在386系统上，代码基址为0x08048000，它允许在代码下方有相当大的堆栈，而仍保持在地址0x08000000上方，从而允许大多数程序使用单个二级页表。

#### 隐藏问题

##### FAT表的名字？

File Allocation Table，文件分配表。

##### 静态链接和动态链接各有什么优缺点？

静态的优点是快，缺点是占空间，而且每次修改都要重新编译；动态的优点是不占空间，可以选择载入，同时每次修改完之后只需要链接即可，缺点就是慢。

### 2.2 实验相关内容

15. #### BPB指定字段的含义

    ```c++
    struct FAT12Header
    {
        char BS_jmpBoot[3];     // 跳转指令
        char BS_OEMName[8];     // 厂商
        ushort BPB_BytsPerSec;  // 每扇区字节数
        uchar BPB_SecPerClus;   // 每簇占用的扇区数
        ushort BPB_RsvdSecCnt;  // Boot占用的扇区数
        uchar BPB_NumFATs;      // FAT表的记录数
        ushort BPB_RootEntCnt;  // 最大根目录文件数
        ushort BPB_TotSec16;    // 每个FAT占用扇区数
        uchar BPB_Media;        // 媒体描述符
        ushort BPB_FATSz16;     // 每个FAT占用扇区数
        ushort BPB_SecPerTrk;   // 每个磁道扇区数
        ushort BPB_NumHeads;    // 磁头数
        uint BPB_HiddSec;       // 隐藏扇区数
        uint BPB_TotSec32;      // 如果BPB_TotSec16是0，则在这里记录
        uchar BS_DrvNum;        // 中断13的驱动器号
        uchar BS_Reserved1;     // 未使用
        uchar BS_BootSig;       // 扩展引导标志
        uint BS_VolID;          // 卷序列号
        char BS_VolLab[11];     // 卷标，必须是11个字符，不足以空格填充
        char BS_FileSysType[8]; // 文件系统类型，必须是8个字符，不足填充空格
        char Boot_Code[448];    // 引导代码
        ushort Magic_Num;       // 0xAA55
    };
    ```

16. #### 如何进入子目录并输出（说明方法调用）

    递归。如果文件类型是目录，那么子目录项会放在数据区。数据区对应的簇里存放的是一个目录项。

    伪代码：

    ```c++
    if (DIRECTORY)
    {
        // 相当于根目录表放到数据区了
        int dir_addr = DATA_SECTION_ADDR + de.cluster_num * header.BPB_BytsPerSec;
        readDirEntry(dir_addr);
    }
    ```

17. #### 如何获得指定文件的内容，即如何获得数据区的内容（比如使用指针等）

    从目录项（无论是存在根目录区还是数据区里，反正是目录项）中获取数据对应的簇号，然后查FAT表，读取对应的数据簇。如果已经结束，就结束；如果没有结束，是个长文件，那就继续查FAT表读数据簇，直到结束为止。

    伪代码：

    ```c++
    fat = readFAT(de.cluster_num);
    // 短文件
    if (fat >= LAST_CLUSTER)
    {
        readDataCluster(de.cluster_num);
    }
    // 坏簇
    else if (fat == BAD_CLUSTER)
    {
        ERR_MSG["BAD_CLUSTER"];
    }
    // 超过一个扇区
    else
    {
        readDataCluster(de.cluster_num);
        // 没结束并且没有坏
        while (fat < LAST_CLUSTER &&
              fat != BAD_CLUSTER)
        {
            fat = readFAT(fat);
            readDataCluster(fat);
        }
    }
    ```

18. #### 如何进行C代码和汇编之间的参数传递和返回值传递、

    在C（或者C++）代码中声明函数，在C中是`extern`（gcc似乎可以不写），在C++中是`extern "C"`，使用C方式调用（因为C++和C对符号表的处理不一样），但并不对函数进行定义。函数定义在汇编代码里，通过global声明，然后在链接时通过链接器进行链接。

    如果是32位系统，通过栈进行传递参数。比如，获取第一个参数：

    ```assembly
    mov eax，[esp + 4]
    ```

    首先为什么是`+ 4`，因为x86是小端模式，栈是从高位向低位延伸的。

    然后为什么是`esp + 4`，因为函数调用时是按照这个顺序压栈的：

    > 实参N\~1→主调函数返回地址→主调函数帧基指针EBP→被调函数局部变量1\~N

    同时，进入callee时，会把caller的ebp压栈，并把caller的esp值赋给callee的ebp作为callee的ebp，接着改变esp值来为函数局部变量预留空间。 

    ![](https://images0.cnblogs.com/i/569008/201405/271644419475745.jpg)

    因为这就是个很简单的函数，函数声明是这样的：

    `void nout(COLOR c, const char* s);`

    但是在gcc 3.4以上的版本中（我用的是5.0），对参数的处理方式不是这样了，是预先计算出需要的参数空间大小，然后直接在栈顶开一块对应大小的空间用于存储参数，然后再进行函数调用。因为ebp占4字节，所以直接变成了`[esp + 4]`。

    参考：[C语言函数调用栈(一)](https://www.cnblogs.com/clover-toeic/p/3755401.html)

    在64位的情况下，前6个参数通过rdi，rsi，rdx，rcx，r8，r9寄存器传递，超出6个后再通过栈rsp传递。

19. #### 汇编代码中对I/O的处理方式，说明指定寄存器所存值的含义

    我怎么记得上次问过这个问题……通过系统调用int 0x80。

    | 名称 | eax       | ebx       | ecx              | edx  |
    | ---- | --------- | --------- | ---------------- | ---- |
    | 输入 | 3，调用号 | 0，STDIN  | 存储输入的地址   | 长度 |
    | 输出 | 4，调用号 | 1，STDOUT | 待输出内容的地址 | 长度 |