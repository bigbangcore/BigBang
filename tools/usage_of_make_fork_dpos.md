1. 将`util/setenv.sh`以及相应的.conf配置文件拷贝到数据文件目录的父目录下；
2. 运行`./setenv.sh b` 以清数据目录、拷贝配置文件到数据目录；
3. 将`util/make-fork-dpos.sh`拷贝到核心钱包所在目录，以确保脚本与钱包的版本匹配；
4. 运行`./make-fork-dpos.sh b` 以开始分支创建、dpos出块过程，稍等几分钟后即可用`listfork`命令查询新建的分支（dpos完成的过程要在45个区块高度后再作查询，可以使用`listtransaction`来查询是否已经产生stake类型的块）。
