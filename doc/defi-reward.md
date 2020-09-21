# 手工验证计算DeFi奖励算法
该示例对应于defi.json，计算第一次发放奖励时每个地址的收益

## 生成地址及持币量

```cpp
CAddress A("1ykdy1dtjw810t8g4ymjqfkkr8yj7m5mr6g20595yhssvr2gva56n2rdn");         10000
CAddress a1("1xnassszea7qevgn44hhg5fqswe78jn95gjd1mx184zr753yt0myfmfxf");        100000
CAddress a11("1nkz90d8jxzcw2rs21tmfxcvpewx7vrz2s0fr033wjm77fchsbzqy7ygb");       100000
CAddress a111("1g9pgwqpcbz3x9czsvtjhc3ed0b24vmyrj5k6zfbh1g82ma67qax55dqy");      100000
CAddress a2("1cem1035c3f7x58808yqge07z8fb72m3czfmzj23hmaxs477tt2ph9yrp");        1
CAddress a21("16br4z57ys0dbgxzesvfhtxrpaq3w3j3bsqgdmkrvye06exg9e74ertx8");       1
CAddress a22("123zbs56t8q5qn6sjchtvhcxc1ngq5v7jvst6hd2nq7hdtfmr691rd6mb");       12000
CAddress a221("1pqmm5nj6qpy436qcytshjra24bhvsysjv6k2xswfzma6w1scwyye25r9");      18000
CAddress a222("12z4t8dzgzmnh3zg1m0b4s2yean8w74cyv0fgrdm7dpq6hbnrq2w0gdwk");      5000
CAddress a3("1h1tn0dcf7cz44cfypmwqjkm7jvxznrdab2ycn0wcyc9q8983fq3akt40");        1000000
CAddress B("1xftkpw9afcgrfac4v3xa9bgb1zsh03bd59npwtyjrqjbtqepr6anrany");         10000
CAddress b1("1thcbm7h2bnyb247wknsvnfsxxd7gw12m2bg5d5gdrmzqmxz93s7k6pvy");        10000
CAddress b2("15n2eszyzk3qm7y04es5fr5fxa3hpbdqqnp1470rmnbmkhpp66mq55qeb");        11000
CAddress b3("1espce7qwvy94qcanecfr7c65fgsjbr1g407ab00p44a65de0mm1mtrqn");        5000
CAddress b4("1mbv5et5sx6x1jejgfh214ze1vryg60n8j6tz30czgzmm3rr461jn0nwm");        50000
CAddress C("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");         8568998
```

## 总挖矿奖励
根据defi.json的makeorigin参数，第一次发放奖励总和计算过程：
```
初始币量 = amount = 10000000
开始挖矿高度 = mintheight = 10
本金变动周期 = supplycycle = 150
发放奖励周期 = rewardcycle = 5
本金变动比例 = mapcoinbasepercent = （259200 + mintheight之前是10%， 777600 + mintheight之前是8%
持币奖励占总奖励比例 = stakerewardpercent = 50% 
推广奖励占总奖励比例 = promotionrewardpercent = 50% 

core的代码中计算任何的token时，都是乘以COIN（1000000）之后
第一个本金变动周期总奖励：10000000 * COIN * 10% = 1000000 * COIN
第一个本金变动周期内每个块的奖励：1000000 * COIN / 150 = 6666.666666666667 * COIN
第一个奖励周期总奖励为：6666.666666 * 5 = 33333.333333 * COIN （最终截断小数部分取整数）
```


## 持币奖励
持币奖励为总奖励的50%，即reward = 33333.333333 * 50% = 16666.666666

```cpp
从所有持币地址中选取大于等于100的地址，按持币量从小到大排序
CAddress a222("12z4t8dzgzmnh3zg1m0b4s2yean8w74cyv0fgrdm7dpq6hbnrq2w0gdwk");      5000                // rank 1
CAddress b3("1espce7qwvy94qcanecfr7c65fgsjbr1g407ab00p44a65de0mm1mtrqn");        5000                // rank 1
CAddress A("1ykdy1dtjw810t8g4ymjqfkkr8yj7m5mr6g20595yhssvr2gva56n2rdn");         10000               // rank 3 
CAddress B("1xftkpw9afcgrfac4v3xa9bgb1zsh03bd59npwtyjrqjbtqepr6anrany");         10000               // rank 3
CAddress b1("1thcbm7h2bnyb247wknsvnfsxxd7gw12m2bg5d5gdrmzqmxz93s7k6pvy");        10000               // rank 3
CAddress b2("15n2eszyzk3qm7y04es5fr5fxa3hpbdqqnp1470rmnbmkhpp66mq55qeb");        11000               // rank 6
CAddress a22("123zbs56t8q5qn6sjchtvhcxc1ngq5v7jvst6hd2nq7hdtfmr691rd6mb");       12000               // rank 7
CAddress a221("1pqmm5nj6qpy436qcytshjra24bhvsysjv6k2xswfzma6w1scwyye25r9");      18000               // rank 8
CAddress b4("1mbv5et5sx6x1jejgfh214ze1vryg60n8j6tz30czgzmm3rr461jn0nwm");        50000               // rank 9
CAddress a1("1xnassszea7qevgn44hhg5fqswe78jn95gjd1mx184zr753yt0myfmfxf");        100000              // rank 10
CAddress a11("1nkz90d8jxzcw2rs21tmfxcvpewx7vrz2s0fr033wjm77fchsbzqy7ygb");       100000              // rank 10
CAddress a111("1g9pgwqpcbz3x9czsvtjhc3ed0b24vmyrj5k6zfbh1g82ma67qax55dqy");      100000              // rank 10
CAddress a3("1h1tn0dcf7cz44cfypmwqjkm7jvxznrdab2ycn0wcyc9q8983fq3akt40");        1000000             // rank 13
CAddress C("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");         8568998             // rank 14

计算总排名：
total = 1 + 1 + 3 + 3 + 3 + 6 + 7 + 8 + 9 + 10 + 10 + 10 + 13 + 14 = 98

单位排名奖励：
reward / 98 = 16666.666666 / 98 = 170.06802720408163

最终每个地址的奖励为（截取6位小数）：
a222 = 1 * (reward / 98) = 170.068027
b3 = 1 * (reward / 98) = 170.068027
A = 3 * (reward / 98) = 510.204081
B = 3 * (reward / 98) = 510.204081
b1 = 3 * (reward / 98) = 510.204081
b2 = 6 * (reward / 98) = 1020.408163
a22 = 7 * (reward / 98) = 1190.476190
a221 = 8 * (reward / 98) = 1360.544217
b4 = 9 * (reward / 98) = 1530.612244
a1 = 10 * (reward / 98) = 1700.680272
a11 = 10 * (reward / 98) = 1700.680272
a111 = 10 * (reward / 98) = 1700.680272
a3 = 13 * (reward / 98) = 2210.884353
C = 14 * (reward / 98) = 2380.952380
```

## 推广奖励

```txt
推广关系：A
             ________________________ A(10000)_______________________
            |                         |                             |
        ___a1(100000)             ___a2(1)_______                   a3(1000000)
        |                        |              |
  ____ a11(100000)              a21(1)    _____a22(12000)_____
  |                                       |                  |
a111(100000)                             a221(18000)        a222(5000)


推广关系：B

______________________________B(10000)_____________________________________
|             |                              |                            |
b1(10000)     b2(11000)                      b3(5000)                     b4(50000)


推广关系：C

                              C(19568998)
```

上三幅图是树状的持币量,下面根据计算每个地址的推广算力,因为是儿子，孙子累加推广收益，从叶子倒着算：

```txt
b1 = 0
b2 = 0
b3 = 0
b4 = 0
B = pow(50000, 1.0 / 3) + 10000 * 10 + 10000 * 10 + 1000 + 5000 * 10 = 37 + 251000 = 251037
C = 0

a111 = 0
a221 = 0
a222 = 0
a21 = 0
a3 = 0
a11 = pow(100000, 1.0 / 3) = 46
a1 = pow(100000 + 100000, 1.0/3) = 58

a22 = pow(18000, 1.0/3) + 5000 * 10 = 26 + 5000 * 10 = 50026
a2 = pow(12000 + 18000 + 5000, 1.0/3) + 1 * 10 = 33 + 10 = 43

A = pow(1000000, 1.0/3) + 10000 * 10 + 290000 + 10000 * 10 + 25002 = 100 + 10000 * 10 + 290000 + 10000 * 10 + 25002 = 515102

总算力是：  251037 + 46 + 58 + 50026 + 43 + 515102 = 816312
单位算力奖励是： reward / 816312 = 16666.666666 / 816312 = 0.020417030088985585
```

下面根据 个人算法 * 单位算力奖励 计算各个地址的推广收益：

```
B = 251037 * 0.020417030088985585 = 5125.429982
a11 = 46 * 0.020417030088985585 = 0.939183
a1 = 58 * 0.020417030088985585 = 1.184187
a22 = 50026  * 0.020417030088985585 = 1021.382347
a2 = 43  * 0.020417030088985585 = 0.877932
A = 515102  * 0.020417030088985585 = 10516.853032
```

## 每个地址总收益

就是推广收益 + 持币收益

```
a2("1cem1035c3f7x58808yqge07z8fb72m3czfmzj23hmaxs477tt2ph9yrp") =    0 + 0.877932                = 0.877932
a222("12z4t8dzgzmnh3zg1m0b4s2yean8w74cyv0fgrdm7dpq6hbnrq2w0gdwk") =  170.068027 + 0              = 170.068027
b3("1espce7qwvy94qcanecfr7c65fgsjbr1g407ab00p44a65de0mm1mtrqn") =    170.068027 + 0              = 170.068027
b1("1thcbm7h2bnyb247wknsvnfsxxd7gw12m2bg5d5gdrmzqmxz93s7k6pvy") =    510.204081 + 0              = 510.204081
b2("15n2eszyzk3qm7y04es5fr5fxa3hpbdqqnp1470rmnbmkhpp66mq55qeb") =    1020.408163 + 0             = 1020.408163
a221("1pqmm5nj6qpy436qcytshjra24bhvsysjv6k2xswfzma6w1scwyye25r9") =  1360.544217 + 0             = 1360.544217
b4("1mbv5et5sx6x1jejgfh214ze1vryg60n8j6tz30czgzmm3rr461jn0nwm") =    1530.612244 + 0             = 1530.612244
a111("1g9pgwqpcbz3x9czsvtjhc3ed0b24vmyrj5k6zfbh1g82ma67qax55dqy") =  1700.680272 + 0             = 1700.680272
a11("1nkz90d8jxzcw2rs21tmfxcvpewx7vrz2s0fr033wjm77fchsbzqy7ygb") =   1700.680272 + 0.939183      = 1701.619455
a1("1xnassszea7qevgn44hhg5fqswe78jn95gjd1mx184zr753yt0myfmfxf") =    1700.680272 + 1.184187      = 1701.864459
a3("1h1tn0dcf7cz44cfypmwqjkm7jvxznrdab2ycn0wcyc9q8983fq3akt40") =    2210.884353 + 0             = 2210.884353
a22("123zbs56t8q5qn6sjchtvhcxc1ngq5v7jvst6hd2nq7hdtfmr691rd6mb") =   1190.476190 + 1021.382347   = 2211.858537
C("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm") =     2380.952380 + 0             = 2380.952380
B("1xftkpw9afcgrfac4v3xa9bgb1zsh03bd59npwtyjrqjbtqepr6anrany") =     510.204081 + 5125.429982    = 5635.634063
A("1ykdy1dtjw810t8g4ymjqfkkr8yj7m5mr6g20595yhssvr2gva56n2rdn") =     510.204081 + 10516.853032   = 11027.057113
```