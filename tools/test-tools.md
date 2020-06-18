# 有用的测试工具用法

## 用GNU gprof分析性能

在CMakeLists.txt里面的CMAKE_CXX_FLAGS的C++编译参数里加入-pg选项

```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -rdynamic -pg")
```

然后，这个参数就是让gcc编译器在编译的时候在项目里的各个函数的调用前后插入桩函数，桩函数记录时间戳来统计时间和调用次数。

然后这个记录会默认写到当前运行目录下一个叫gmon.out的文件里。

程序运行一段时间以后，停止程序。就可以通过gmon.out查看这段时间的统计记录结果。

这时候就需要gprof命令行工具来通过gmon.out来生成测试报告了:

```bash
gprof ./bigbang ./gmon.out > report.txt
```

把结果导入到一个report.txt的文件里。

### 参考资料

- https://www.tutorialspoint.com/unix_commands/gprof.htm
- https://blog.csdn.net/luchengtao11/article/details/74910585

## Valgrind工具集使用

Valgrind包含下列工具：

- memcheck：检查程序中的内存问题，如泄漏、越界、非法指针等。
- callgrind：检测程序代码的运行时间和调用过程，以及分析程序性能。
- cachegrind：分析CPU的cache命中率、丢失率，用于进行代码优化。
- helgrind：用于检查多线程程序的竞态条件。
- massif：堆栈分析器，指示程序中使用了多少堆内存等信息。
- lackey：
- nulgrind：

这几个工具的使用是通过命令：valgrand --tool=name 程序名来分别调用的，当不指定tool参数时默认是 --tool=memcheck

### 检查内存泄漏，内存越界，非法指针

这里就不写教程了，以下这些连接的内容够用了：

- https://blog.csdn.net/weixin_43762200/article/details/90957976

### 分析性能，多线程性能

callgrind， 类似gprof，但是不需要类似gprof那样需要加入编译参数-pg。但加上调试选项是推荐的。Callgrind收集程序运行时的一些数据，建立函数调用关系图，还可以有选择地进行cache模拟。在运行结束时，它会把分析数据写入一个文件。callgrind_annotate可以把这个文件的内容转化成可读的形式。

详细阅读 https://www.cnblogs.com/zengkefu/p/5642991.html


### 分析CPU缓存命中率

Cachegrind， Cache分析器，它模拟CPU中的一级缓存I1，Dl和二级缓存，能够精确地指出程序中cache的丢失和命中。如果需要，它还能够为我们提供cache丢失次数，内存引用次数，以及每行代码，每个函数，每个模块，整个程序产生的指令数。这对优化程序有很大的帮助。

仔细阅读cachegrind部分 https://blog.csdn.net/sunmenggmail/article/details/10543483

### 分析多线程竞争加锁的问题

Helgrind，它主要用来检查多线程程序中出现的竞争问题。Helgrind寻找内存中被多个线程访问，而又没有一贯加锁的区域，这些区域往往是线程之间失去同步的地方，而且会导致难以发掘的错误。Helgrind实现了名为“Eraser”的竞争检测算法，并做了进一步改进，减少了报告错误的次数。不过，Helgrind仍然处于实验阶段。

仔细阅读HelGrind部分 https://blog.csdn.net/sunmenggmail/article/details/10543483

### 参考资料

- https://www.valgrind.org/


