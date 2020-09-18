# 手工验证计算DeFi奖励算法

## 生成地址及持币量

```cpp
CAddress A("1ykdy1dtjw810t8g4ymjqfkkr8yj7m5mr6g20595yhssvr2gva56n2rdn");         10000               // rank 3 
CAddress a1("1xnassszea7qevgn44hhg5fqswe78jn95gjd1mx184zr753yt0myfmfxf");        100000              // rank 10
CAddress a11("1nkz90d8jxzcw2rs21tmfxcvpewx7vrz2s0fr033wjm77fchsbzqy7ygb");       100000              // rank 10
CAddress a111("1g9pgwqpcbz3x9czsvtjhc3ed0b24vmyrj5k6zfbh1g82ma67qax55dqy");      100000              // rank 10
CAddress a2("1cem1035c3f7x58808yqge07z8fb72m3czfmzj23hmaxs477tt2ph9yrp");        1
CAddress a21("16br4z57ys0dbgxzesvfhtxrpaq3w3j3bsqgdmkrvye06exg9e74ertx8");       1
CAddress a22("123zbs56t8q5qn6sjchtvhcxc1ngq5v7jvst6hd2nq7hdtfmr691rd6mb");       12000               // rank 7
CAddress a221("1pqmm5nj6qpy436qcytshjra24bhvsysjv6k2xswfzma6w1scwyye25r9");      18000               // rank 8
CAddress a222("12z4t8dzgzmnh3zg1m0b4s2yean8w74cyv0fgrdm7dpq6hbnrq2w0gdwk");      5000                // rank 1
CAddress a3("1h1tn0dcf7cz44cfypmwqjkm7jvxznrdab2ycn0wcyc9q8983fq3akt40");        1000000             // rank 13
CAddress B("1xftkpw9afcgrfac4v3xa9bgb1zsh03bd59npwtyjrqjbtqepr6anrany");         10000               // rank 3
CAddress b1("1thcbm7h2bnyb247wknsvnfsxxd7gw12m2bg5d5gdrmzqmxz93s7k6pvy");        10000               // rank 3
CAddress b2("15n2eszyzk3qm7y04es5fr5fxa3hpbdqqnp1470rmnbmkhpp66mq55qeb");        11000               // rank 6
CAddress b3("1espce7qwvy94qcanecfr7c65fgsjbr1g407ab00p44a65de0mm1mtrqn");        5000                // rank 1
CAddress b4("1mbv5et5sx6x1jejgfh214ze1vryg60n8j6tz30czgzmm3rr461jn0nwm");        50000               // rank 9
CAddress C("1mwxyqkvdd4xm8py7gtv13j9rxzs8d7k2hgsjy1s4b0znkd04ryzpa56z");         19568998            // rank 14
                                                                                                    // total = 3 + 10*3 + 7 + 8 + 1 + 13 + 3 * 2 + 6 + 1 + 9 + 14
```

## 持币奖励

```cpp
CAddress A("1ykdy1dtjw810t8g4ymjqfkkr8yj7m5mr6g20595yhssvr2gva56n2rdn");         10000               // rank 3 
CAddress a1("1xnassszea7qevgn44hhg5fqswe78jn95gjd1mx184zr753yt0myfmfxf");        100000              // rank 10
CAddress a11("1nkz90d8jxzcw2rs21tmfxcvpewx7vrz2s0fr033wjm77fchsbzqy7ygb");       100000              // rank 10
CAddress a111("1g9pgwqpcbz3x9czsvtjhc3ed0b24vmyrj5k6zfbh1g82ma67qax55dqy");      100000              // rank 10
CAddress a2("1cem1035c3f7x58808yqge07z8fb72m3czfmzj23hmaxs477tt2ph9yrp");        1
CAddress a21("16br4z57ys0dbgxzesvfhtxrpaq3w3j3bsqgdmkrvye06exg9e74ertx8");       1
CAddress a22("123zbs56t8q5qn6sjchtvhcxc1ngq5v7jvst6hd2nq7hdtfmr691rd6mb");       12000               // rank 7
CAddress a221("1pqmm5nj6qpy436qcytshjra24bhvsysjv6k2xswfzma6w1scwyye25r9");      18000               // rank 8
CAddress a222("12z4t8dzgzmnh3zg1m0b4s2yean8w74cyv0fgrdm7dpq6hbnrq2w0gdwk");      5000                // rank 1
CAddress a3("1h1tn0dcf7cz44cfypmwqjkm7jvxznrdab2ycn0wcyc9q8983fq3akt40");        1000000             // rank 13
CAddress B("1xftkpw9afcgrfac4v3xa9bgb1zsh03bd59npwtyjrqjbtqepr6anrany");         10000               // rank 3
CAddress b1("1thcbm7h2bnyb247wknsvnfsxxd7gw12m2bg5d5gdrmzqmxz93s7k6pvy");        10000               // rank 3
CAddress b2("15n2eszyzk3qm7y04es5fr5fxa3hpbdqqnp1470rmnbmkhpp66mq55qeb");        11000               // rank 6
CAddress b3("1espce7qwvy94qcanecfr7c65fgsjbr1g407ab00p44a65de0mm1mtrqn");        5000                // rank 1
CAddress b4("1mbv5et5sx6x1jejgfh214ze1vryg60n8j6tz30czgzmm3rr461jn0nwm");        50000               // rank 9
CAddress C("1mwxyqkvdd4xm8py7gtv13j9rxzs8d7k2hgsjy1s4b0znkd04ryzpa56z");         19568998            // rank 14
```

## 推广奖励

```txt
             ________________________ A(10000)_______________________
            |                         |                             |
        ___a1(100000)             ___a2(1)_______                   a3(1000000)
        |                        |              |
  ____ a11(100000)              a21(1)    _____a22(12000)_____
  |                                       |                  |
a111(100000)                             a221(18000)        a222(5000)
```

```txt
______________________________B(10000)_____________________________________
|             |                              |                            |
b1(10000)     b2(11000)                      b3(5000)                     b4(50000)
```

```txt
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
```

下面根据 个人算法/总算力 * 奖励 计算各个地址的推广收益：

```
B = 251037 / 816312 * nReward
a11 = 46 / 816312 * nReward
a1 = 58 / 816312 * nReward
a22 = 50026 / 816312 * nReward
a2 = 43 / 816312 * nReward
A = 515102 / 816312 * nReward
```

## 每个地址总收益

就是推广收益 + 持币收益

```

```