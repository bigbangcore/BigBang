#实现原理：
#A、双方协商汇率
#B、跨链交易不是要把A链的token发送到B链上存储，或者将B莲的token发送到B链上存储，他们之间的交易过程，更像是a在A链上为b创建了一个账户且向其账户中转了一笔账，b也在B链中为a创建了一个账户且向其账户中转了一笔账，而交易过程就是在协商的过程
#C、下边将以安全主链与分支链A之间的交易过程为例进行描述

#1、先完成分支的创建工作，假设我妈现在创建了AChain和BChain两条分支，token名称分别为BA和BB，所以现在我们一共有三条链--主链MChain、支链AChain和BChain
#2、先来进行MChain与AChain之间的跨链交易过程，我们用Mchain的100BIG换AChain的200BA
#2.1 MChain先向自己转出100BIG，锁定块数设置为100,也就是在当前高度（如105）加上100块高度（即205）之前，锁定的这100BIG只能转给AChain，这个限制通过参数-d来控制，-d的值为AChai上的指定钱包地址
#命令：m_hash=bigbang sendfrom $m_addr $m_addr 100 1 -f=$m_fork -d=$s_addr -l=100

#2.2 AChain向自己转出200BA，锁定块数设置为50,假设当前高度为105,那么解锁高度为155，锁定的这200BA只能转给MChain，-d的值为MChain上的指定钱包地址
#命令：s_hash=bigbang sendfrom $s_addr $s_addr 200 1 -f=$s_fork -d=$m_addr -l=50

#2.2.1 双方分别检查对方交易hash，确认双方的交易不管是额度上、锁定的块高度上都要达成一定的共识，否则有可能造成不必要的损失
#命令：bigbang gettransaction m_hash
#命令：bigbang gettransaction s_hash

#2.3 MChain的用户对前面的交易进行签名，返回主签名结果
#命令：m_signRe=bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_sign

#2.4 AChain的用户对前面的交易过程进行签名，返回从签名结果
#命令：s_signRe=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_sign)

#2.5 签名结束后，双方相互验证对方的签名数据是否正确，验证成功，返回OK
#命令1：m_verify_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_verify -m=$m_signRe -s=$s_signRe)
#命令2：s_verify_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_verify -m=$m_signRe -s=$s_signRe)

#2.6 交易的双方都开始广播信息，广播过程要求——谁锁定的数据块少于另一个，谁先广播。否则，无法保证交易的安全性。返回交易hash
#命令1：m_submit_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_submit -m=$m_signRe -s=$s_signRe)
#命令2：s_submit_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_submit -m=$m_signRe -s=$s_signRe)

#2.7 下面我们来检查一下交易完成后的情况
#命令1：bigbang getbalance -a=$m_addr -f=$m_fork
#命令2：bigbang getbalance -a=$s_addr -f=$s_fork

#需要特别注意的问题：
#1、在交易双方各自锁定了自己的高度和交易额度等信息后，双方一定要查看一下对方的交易hash，通过<bigbang gettransaction 交易hash>的返回结果来认真查阅
#2、各自签名后，将签名结果发给对方，然后双方进行验证工作，验证成功会返回OK
#3、在正式提交，将数据广播出去的时候，要求锁定高度低的用户先把自己的签名发送给对方，锁定高度高的用户应在锁定高度低的高度到达之前广播出去，否则可能造成损失

