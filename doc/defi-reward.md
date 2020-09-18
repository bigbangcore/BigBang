# 手工验证计算DeFi奖励算法

## 生成地址及持币量

```cpp
CAddress A("1632srrskscs1d809y3x5ttf50f0gabf86xjz2s6aetc9h9ewwhm58dj3");         10000               // rank 3 
CAddress a1("1f1nj5gjgrcz45g317s1y4tk18bbm89jdtzd41m9s0t14tp2ngkz4cg0x");        100000              // rank 10
CAddress a11("1pmj5p47zhqepwa9vfezkecxkerckhrks31pan5fh24vs78s6cbkrqnxp");       100000              // rank 10
CAddress a111("1bvaag2t23ybjmasvjyxnzetja0awe5d1pyw361ea3jmkfdqt5greqvfq");      100000              // rank 10
CAddress a2("1ab1sjh07cz7xpb0xdkpwykfm2v91cvf2j1fza0gj07d2jktdnrwtyd57");        1
CAddress a21("1782a5jv06egd6pb2gjgp2p664tgej6n4gmj79e1hbvgfgy3t006wvwzt");       1
CAddress a22("1c7s095dcvzdsedrkpj6y5kjysv5sz3083xkahvyk7ry3tag4ddyydbv4");       12000               // rank 7
CAddress a221("1ytysehvcj13ka9r1qhh9gken1evjdp9cn00cz0bhqjqgzwc6sdmse548");      18000               // rank 8
CAddress a222("16aaahq32cncamxbmvedjy5jqccc2hcpk7rc0h2zqcmmrnm1g2213t2k3");      5000                // rank 1
CAddress a3("1vz7k0748t840bg3wgem4mhf9pyvptf1z2zqht6bc2g9yn6cmke8h4dwe");        1000000             // rank 13
CAddress B("1fpt2z9nyh0a5999zrmabg6ppsbx78wypqapm29fsasx993z11crp6zm7");         10000               // rank 3
CAddress b1("1rampdvtmzmxfzr3crbzyz265hbr9a8y4660zgpbw6r7qt9hdy535zed7");        10000               // rank 3
CAddress b2("1w0k188jwq5aenm6sfj6fdx9f3d96k20k71612ddz81qrx6syr4bnp5m9");        11000               // rank 6
CAddress b3("196wx05mee1zavws828vfcap72tebtskw094tp5sztymcy30y7n9varfa");        5000                // rank 1
CAddress b4("19k8zjwdntjp8avk41c8aek0jxrs1fgyad5q9f1gd1q2fdd0hafmm549d");        50000               // rank 9
CAddress C("1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm");         19568998            // rank 14
                                                                                                    // total = 3 + 10*3 + 7 + 8 + 1 + 13 + 3 * 2 + 6 + 1 + 9 + 14
```

```txt
             ________________________ A_______________________
            |                         |                      |
        ____a1                    ___ a2_______              a3
        |                        |            |
  ____ a11                      a21     _____a22_____
  |                                     |           |
a111                                  a221        a222
```

```txt
______________________________B_____________________________________
|             |                              |                     |
b1            b2                            b3                    b4
```

```txt
                              C
```

