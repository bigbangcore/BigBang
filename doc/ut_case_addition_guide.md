### 编译/运行单元测试

单元测试已经通过`./CMakeLists.txt`和`./test/CMakeLists.txt`集成在BigBangCoreWallet
项目中，target设置为构建所有或者仅仅构建指定的test_big即可构建该单元测试。

通过加载`./test/test_big`来手工运行bigbang测试。如果单元测试文件或被测试的代
码文件发生变化，重新构建单元测试即可。

有2种方式来添加更多的bigbang单元测试，第一种是向位于`test/`目录下存在的.cpp文件
加入`BOOST_AUTO_TEST_CASE`函数；另一种是添加新的.cpp文件，它实现了新的
`BOOST_AUTO_TEST_SUITE`节。

### 运行单独的单元测试

test_big有一些内建的命令行参数，例如，仅仅运行uint256_tests并且输出详细的信息：

    test_big --log_level=all --run_test=uint256_tests

... 或者只想运行uint256_tests中的methods测试子项:

    test_big --log_level=all --run_test=uint256_tests/methods

运行 `test_big --help` 可以获取详细的帮助列表信息.

### 添加单元测试的说明

这个目录下的源代码文件均为单元测试案例。Boost包含一个单元测试框架，
因为bigbang已经使用boost，简单地使用这个框架而不需要另外去配置
一些其它的框架是有意义的，这样可以更加方便快捷地创建单元测试。

构建系统被设置成编译一个可执行程序名字为`test_big`，它运行所有的单元
测试。主要的源文件名字为`test_big.cpp`。欲添加一个新的单元测试文件到
我们的测试套件中，你需要要将这些文件名列表加入到`./test/CMakeLists.txt`
中。我们以一个单元测试文件来囊括一个类或一个源代码文件的测试为一般性的原则。
文件的命名规则为：`<source_filename>_tests.cpp`，这些文件封装它们的测试在
一个名为：`<source_filename>_tests`的测试套件中。`uint256_tests.cpp`可以
作为一个具体的测试单元例子参考。
