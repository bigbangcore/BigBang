通过模板方式实现的跨链交易测试流程

./bigbang -daemon

## 导入挖矿私钥和模板
./bigbang importprivkey 8102db22dcc095d4d685613ca89a5d938e2de39954b5aa46f975ded23d9c9d69 123456
./bigbang importprivkey 27ee1dce5fe525a38ffb3ee1515206451fde7443e422cc9a26ae0d65766eb608 123456
./bigbang importprivkey 1f6c18ed562d9585de0a43cdefc551bdd1b82878cbb11c3b91209d0eff985b73 123456
./bigbang importprivkey 4414d207483bee46fc116b57925efa26e5313782af19a9ba34c9d805b8ee25d2 123456
./bigbang importprivkey 9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee 123456
./bigbang importprivkey bf0ebca9be985104e88b6a24e54cedbcdd1696a8984c8fdd7bc96917efb5a1ed 123456

## 解锁钱包
./bigbang unlockkey 5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2 123456
./bigbang unlockkey 8656f962ae73724ea2234e6aa5ea38d059b1e702c8d6d6fcfbdde2cda66d39e4 123456
./bigbang unlockkey abe946e6cc8f250d744936c04cf35730b8107937abd9ec5f721b1c94c34d8952 123456
./bigbang unlockkey fb8b609486b2e77400cbfacf0121079596d8ae520223c299494cee5e4001a049 123456
./bigbang unlockkey da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49 123456
./bigbang unlockkey 575f2041770496489120bb102d9dd55f5e75b0c4aa528d5762b92b59acd6d939 123456

## 获得钱包地址
./bigbang getpubkeyaddress 5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2
1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h--钱包地址1

./bigbang addnewtemplate mint "{\"mint\":\"5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2\",\"spent\":\"1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h\"}"

20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8
 
 # 开始挖矿
./bigbang miner 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h 8102db22dcc095d4d685613ca89a5d938e2de39954b5aa46f975ded23d9c9d69
./bigbang getpubkeyaddress 8656f962ae73724ea2234e6aa5ea38d059b1e702c8d6d6fcfbdde2cda66d39e4
1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y--钱包地址2

./bigbang getpubkeyaddress abe946e6cc8f250d744936c04cf35730b8107937abd9ec5f721b1c94c34d8952
1aa4mvgwm3gdq4qzcv6nkey8gq0r5fwtcr0v4jx0d4p7wssj6x6nhj7kw--钱包地址3

./bigbang getpubkeyaddress fb8b609486b2e77400cbfacf0121079596d8ae520223c299494cee5e4001a049
196g02g2yxs64k6e24c155bprjtage881szxcp03mwys8d530hfxyqk7f--钱包地址4

./bigbang getpubkeyaddress da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49
1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm--钱包地址5

./bigbang getpubkeyaddress 575f2041770496489120bb102d9dd55f5e75b0c4aa528d5762b92b59acd6d939
177cxdb2s5ewp4nwdaanc9c3nbsfxb79d22xj14a8jr27eg90bxbqh4eb--钱包地址6

## 添加挖矿模板
./bigbang addnewtemplate mint "{\"mint\":\"5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2\",\"spent\":\"1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h\"}"
20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8
 
## 开始挖矿
./bigbang miner 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h 8102db22dcc095d4d685613ca89a5d938e2de39954b5aa46f975ded23d9c9d69


## create fork
./bigbang getblockhash 10
[
    "0c06b82cf444a8b0bf02515f347afc3dd4693d36c69bb7fe94b1797f633d53c8"
]

## 根据预定支链信息生成支链 ID 和支链参数数据
./bigbang makeorigin 0c06b82cf444a8b0bf02515f347afc3dd4693d36c69bb7fe94b1797f633d53c8 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h 100000000 BigBangA BA 15
{
    "hash" : "557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783",
    "hex" : "010000ff12ffe45cc8533d637f79b194feb79bc6363d69d43dfc7a345f5102bfb0a844f42cb8060c0000000000000000000000000000000000000000000000000000000000000000708001000000e10842696742616e6741e20242414301a46400000000000000a5c0e1e40000000000e62101e21d19ac52602eca9f9f70f5b43abfff29ed552b3d4f08478f50829c6c3b8c5ac78f49a251e0d1a3780dcd60470bcc4cf5fad86ea9acdbe48f1267ecff85940992880a0000000100000112ffe45c0000000000000000000000000000000000000000000000000000000000000000000000000001e21d19ac52602eca9f9f70f5b43abfff29ed552b3d4f08478f50829c6c3b8c5a00407a10f35a000000000000000000000842696742616e67410000405f48ea67d62d714e35929f864e74ccbe89bc9512c0ed8b739ee0f0f2a8c12a5fec472051a45be8175bd71b25d7b6b4c4129f790d92804f527d2da6208deaf104"
}

## 添加创建支链模板(./bigbang addnewtemplate fork '{"redeem":"赎回地址","fork":"上一步生成的支链 ID"}')
./bigbang addnewtemplate fork "{\"redeem\":\"1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h\",\"fork\":\"557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783\"}"
20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8

## 创建分叉交易
./bigbang sendfrom 20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8  20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8 1000 5 -d=010000ff12ffe45cc8533d637f79b194feb79bc6363d69d43dfc7a345f5102bfb0a844f42cb8060c0000000000000000000000000000000000000000000000000000000000000000708001000000e10842696742616e6741e20242414301a46400000000000000a5c0e1e40000000000e62101e21d19ac52602eca9f9f70f5b43abfff29ed552b3d4f08478f50829c6c3b8c5ac78f49a251e0d1a3780dcd60470bcc4cf5fad86ea9acdbe48f1267ecff85940992880a0000000100000112ffe45c0000000000000000000000000000000000000000000000000000000000000000000000000001e21d19ac52602eca9f9f70f5b43abfff29ed552b3d4f08478f50829c6c3b8c5a00407a10f35a000000000000000000000842696742616e67410000405f48ea67d62d714e35929f864e74ccbe89bc9512c0ed8b739ee0f0f2a8c12a5fec472051a45be8175bd71b25d7b6b4c4129f790d92804f527d2da6208deaf104
5ce764263a62c6687ea8bb8b5d12e054dfb483f1896f16fb6cbb31a4b6a6c81b

./bigbang stop
./bigbang -addfork=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783 -daemon

## 列出所有分支
./bigbang listfork
[
    {
        "fork" : "557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783",
        "name" : "BigBangA",
        "symbol" : "BA",
        "isolated" : true,
        "private" : false,
        "enclosed" : false,
        "owner" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h"
    },
    {
        "fork" : "92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f",
        "name" : "BigBang Network",
        "symbol" : "BIG",
        "isolated" : true,
        "private" : false,
        "enclosed" : false,
        "owner" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm"
    }
]

## 接下来是关于跨链交易的操作，先看看两条链上的Token分布情况
./bigbang getbalance
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y",
        "avail" : 15.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 745000000.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8",
        "avail" : 1565.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8",
        "avail" : 1000.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

./bigbang getbalance -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 100000000.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

## 添加跨链交易模板，返回一个模板地址
* addr_m:参与跨链交易的主钱包地址 1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm
* addr_s:参与跨链交易的从钱包地址 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h
* height_m:约定的主钱包所在支链锁定的区块高度（不是锁定的数量，所以设置这个值之前，请先查看当前区块高度是多少）
* height_s:约定的从钱包所在支链锁定的指定区块高度
* fork_m:92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f
* fork_s:557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783

./bigbang addnewtemplate exchange "{\"addr_m\":\"1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm\",\"addr_s\":\"1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h\",\"height_m\":400,\"height_s\":410,\"fork_m\":\"92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f\",\"fork_s\":\"557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783\"}"
20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3

## 交易双方朝跨链交易模板地址转账，Token转出地址可以是任意有Token的地址，记得后面要带fork参数-f来指定所操作的是哪一条支链
./bigbang sendfrom 1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 13.345 1 -f="92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f"
5ce76b63c1d8b3cc62db8b794c51988daa2b45b50043a0099c6975125e19ed05

./bigbang sendfrom 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 9.875 1 -f="557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783"
5ce76b9825f8a09616c709fc2298cde2ed95631d85e74659a38529c7d5174083

## 查看交易情况
./bigbang gettransaction 5ce76b63c1d8b3cc62db8b794c51988daa2b45b50043a0099c6975125e19ed05
{
    "transaction" : {
        "txid" : "5ce76b63c1d8b3cc62db8b794c51988daa2b45b50043a0099c6975125e19ed05",
        "version" : 1,
        "type" : "token",
        "time" : 1558670179,
        "lockuntil" : 0,
        "anchor" : "89e0a4ede2b32e670b1f6ed0840b4e04cca0b9aa2cab8314639a5bbfd009d922",
        "vin" : [
            {
                "txid" : "5a586f845d2b565547cc9f03fd98f931bf3a695c103c380f377f56f021485300",
                "vout" : 0
            }
        ],
        "sendto" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "amount" : 13.345000,
        "txfee" : 1.000000,
        "data" : "",
        "sig" : "99d9b3081a0a11742e0d1b6210f450fa74a9cd1a6f10ce6cfd6ac66d5befb25ef4906120518d7733e77c631ad5669dee672dfb3b5cad96db21cebc236b1d190b",
        "fork" : "92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f",
        "confirmations" : 1
    }
}

./bigbang gettransaction 5ce76b9825f8a09616c709fc2298cde2ed95631d85e74659a38529c7d5174083
{
    "transaction" : {
        "txid" : "5ce76b9825f8a09616c709fc2298cde2ed95631d85e74659a38529c7d5174083",
        "version" : 1,
        "type" : "token",
        "time" : 1558670232,
        "lockuntil" : 0,
        "anchor" : "fd29796e92ee4a86588f8667ed654defe72b8fc647f88b959d5544cf49f89fc8",
        "vin" : [
            {
                "txid" : "5ce4ff12391e49080f93136d14da797c900777e37c696b326a23aa6588f332c4",
                "vout" : 0
            }
        ],
        "sendto" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "amount" : 9.875000,
        "txfee" : 1.000000,
        "data" : "",
        "sig" : "604c27240579d6d236e7fe40a8936a09d8ed1ef66d58a33aaa78f80149875f773b1d0ce88e671b9ce60c4524c15febca5c692168987e1815325f26b40301b302",
        "fork" : "557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783",
        "confirmations" : 0
    }
}

## 验证跨链交易模板，查看各项信息是否正常
./bigbang validateaddress 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3
{
    "isvalid" : true,
    "addressdata" : {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "ismine" : true,
        "type" : "template",
        "template" : "exchange",
        "templatedata" : {
            "type" : "exchange",
            "hex" : "060001498b63009dfb70f7ee0902ba95cc171f7d7a97ff16d89fd96e1f1b9e7d5f91da01e21d19ac52602eca9f9f70f5b43abfff29ed552b3d4f08478f50829c6c3b8c5ac8000000d20000008f49a251e0d1a3780dcd60470bcc4cf5fad86ea9acdbe48f1267ecff8594099283c7bbf3ad21fcef715b73d0a3dbf643b2f25e7248fe4ae194a8698652847355",
            "exchange" : {
                "spend_m" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
                "spend_s" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
                "height_m" : 200,
                "height_s" : 210,
                "fork_m" : "92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f",
                "fork_s" : "557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783"
            }
        }
    }
}

## 使用公钥对模板地址进行签名，此时钱包必须处于打开状态，否则签名不成功。-a这个参数表示是对地址的签名。下边是主签名
./bigbang signmessage da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 -a
{
    "code" : -405,
    "message" : "Key is locked"
}
./bigbang unlockkey da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49 123456
./bigbang signmessage da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 -a
1b8c6b0807472627cec2f77b2a33428539b64493f7f1b9f7be4dfbdee72f9aeb3b5763fa39ce5df2425d8d530698de9c1cea993bccfce6596901b6b8cb054103 --sm


## 从签名
./bigbang signmessage 5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 -a
{
    "code" : -405,
    "message" : "Key is locked"
}
./bigbang unlockkey 5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2 123456
./bigbang signmessage 5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 -a
a51a925a50a7101ca25c8165050b61421f9e54aad5b0cfb4a4947da82f5fb62fec8e96fb80bd697db33f391bf1f875179d446ac08829f85cf8fe6483ec9acf0c --ss

## 相互验证对方的签名，返回true表示信息无误，-a参数表示该签名是针对地址的
./bigbang verifymessage 5a8c3b6c9c82508f47084f3d2b55ed29ffbf3ab4f5709f9fca2e6052ac191de2 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 a51a925a50a7101ca25c8165050b61421f9e54aad5b0cfb4a4947da82f5fb62fec8e96fb80bd697db33f391bf1f875179d446ac08829f85cf8fe6483ec9acf0c -a

./bigbang verifymessage da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 1b8c6b0807472627cec2f77b2a33428539b64493f7f1b9f7be4dfbdee72f9aeb3b5763fa39ce5df2425d8d530698de9c1cea993bccfce6596901b6b8cb054103 -a

## 先看看在我们的跨链交易模板上，参与交易的双方各存入了有多少Token
./bigbang getbalance -a=20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3
[
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 13.345000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

./bigbang getbalance -a=20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
[
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 9.875000,
        "locked" : 0.000000,
        "unconfirmed" : 9.875000
    }
]

## 列出forks
./bigbang listfork
[
    {
        "fork" : "557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783",
        "name" : "BigBangA",
        "symbol" : "BA",
        "isolated" : true,
        "private" : false,
        "enclosed" : false,
        "owner" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h"
    },
    {
        "fork" : "92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f",
        "name" : "BigBang Network",
        "symbol" : "BIG",
        "isolated" : true,
        "private" : false,
        "enclosed" : false,
        "owner" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm"
    }
]

## 从跨链交易模板上将Token转走
* 命令：./bigbang sendfrom "模板地址" "钱包地址" "要转走的Token" "交易费" "-sm:主签名 " "-ss:从签名" "-f:交易对方的Token所在的支链hash"

 ./bigbang sendfrom 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h 13.345 1 -sm="1b8c6b0807472627cec2f77b2a33428539b64493f7f1b9f7be4dfbdee72f9aeb3b5763fa39ce5df2425d8d530698de9c1cea993bccfce6596901b6b8cb054103" -ss="a51a925a50a7101ca25c8165050b61421f9e54aad5b0cfb4a4947da82f5fb62fec8e96fb80bd697db33f391bf1f875179d446ac08829f85cf8fe6483ec9acf0c" -f="92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f"
{
    "code" : -401,
    "message" : "Failed to create transaction"
}

* 注意：将跨链交易模板地址中的钱转走的时候，要注意的一点是——交易费需要在模板地址余额中扣除

./bigbang sendfrom 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h 12.345 1 -sm="1b8c6b0807472627cec2f77b2a33428539b64493f7f1b9f7be4dfbdee72f9aeb3b5763fa39ce5df2425d8d530698de9c1cea993bccfce6596901b6b8cb054103" -ss="a51a925a50a7101ca25c8165050b61421f9e54aad5b0cfb4a4947da82f5fb62fec8e96fb80bd697db33f391bf1f875179d446ac08829f85cf8fe6483ec9acf0c" -f="92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f"
5ce79ba78b44f8710710c8e35b8d3b725c2f994d4038b74fda84f0b061c01a76

./bigbang getbalance -a=1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 12.345000,
        "locked" : 0.000000,
        "unconfirmed" : 12.345000
    }
]

./bigbang getbalance -a=1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 99999989.125000,
        "locked" : 0.000000,
        "unconfirmed" : 99999989.125000
    }
]

* 另一个用户的转账情况
 ./bigbang sendfrom 20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm 8.875 1 -sm="1b8c6b0807472627cec2f77b2a33428539b64493f7f1b9f7be4dfbdee72f9aeb3b5763fa39ce5df2425d8d530698de9c1cea993bccfce6596901b6b8cb054103" -ss="a51a925a50a7101ca25c8165050b61421f9e54aad5b0cfb4a4947da82f5fb62fec8e96fb80bd697db33f391bf1f875179d446ac08829f85cf8fe6483ec9acf0c" -f="557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783"
5ce79c872d1aee489291fd97270639a4631014334feb37a5df7fcb27904c0b9f

./bigbang getbalance -a=1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm
[
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 744999985.655000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

./bigbang getbalance -a=1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
[
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 8.875000,
        "locked" : 0.000000,
        "unconfirmed" : 8.875000
    }
]

## 查看交易
./bigbang gettransaction 5ce79ba78b44f8710710c8e35b8d3b725c2f994d4038b74fda84f0b061c01a76
{
    "transaction" : {
        "txid" : "5ce79ba78b44f8710710c8e35b8d3b725c2f994d4038b74fda84f0b061c01a76",
        "version" : 1,
        "type" : "token",
        "time" : 1558682535,
        "lockuntil" : 0,
        "anchor" : "c8d7b6d138cc5e6d4871b92f5a6323eb32f02b7d86c1581333968689d929a28b",
        "vin" : [
            {
                "txid" : "5ce76b63c1d8b3cc62db8b794c51988daa2b45b50043a0099c6975125e19ed05",
                "vout" : 0
            }
        ],
        "sendto" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "amount" : 12.345000,
        "txfee" : 1.000000,
        "data" : "",
        "sig" : "01498b63009dfb70f7ee0902ba95cc171f7d7a97ff16d89fd96e1f1b9e7d5f91da01e21d19ac52602eca9f9f70f5b43abfff29ed552b3d4f08478f50829c6c3b8c5ac8000000d20000008f49a251e0d1a3780dcd60470bcc4cf5fad86ea9acdbe48f1267ecff8594099283c7bbf3ad21fcef715b73d0a3dbf643b2f25e7248fe4ae194a8698652847355400000001b8c6b0807472627cec2f77b2a33428539b64493f7f1b9f7be4dfbdee72f9aeb3b5763fa39ce5df2425d8d530698de9c1cea993bccfce6596901b6b8cb05410340000000a51a925a50a7101ca25c8165050b61421f9e54aad5b0cfb4a4947da82f5fb62fec8e96fb80bd697db33f391bf1f875179d446ac08829f85cf8fe6483ec9acf0c400000008c3fa5efc865e2ad49ce0c48bda370334ba608243229064c21c5719ff06d58b672a753e47367d96c9eda016426bcf7db74e4d0ed4db62c2ecda73875b2307806",
        "fork" : "92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f",
        "confirmations" : 1
    }
}

./bigbang gettransaction 5ce79c872d1aee489291fd97270639a4631014334feb37a5df7fcb27904c0b9f
{
    "transaction" : {
        "txid" : "5ce79c872d1aee489291fd97270639a4631014334feb37a5df7fcb27904c0b9f",
        "version" : 1,
        "type" : "token",
        "time" : 1558682759,
        "lockuntil" : 0,
        "anchor" : "76c378a30ec85c1561a30a4ec35f0ab7a9bc03512bbb16aa96ee3432ab22a581",
        "vin" : [
            {
                "txid" : "5ce76b9825f8a09616c709fc2298cde2ed95631d85e74659a38529c7d5174083",
                "vout" : 0
            }
        ],
        "sendto" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "amount" : 8.875000,
        "txfee" : 1.000000,
        "data" : "",
        "sig" : "01498b63009dfb70f7ee0902ba95cc171f7d7a97ff16d89fd96e1f1b9e7d5f91da01e21d19ac52602eca9f9f70f5b43abfff29ed552b3d4f08478f50829c6c3b8c5ac8000000d20000008f49a251e0d1a3780dcd60470bcc4cf5fad86ea9acdbe48f1267ecff8594099283c7bbf3ad21fcef715b73d0a3dbf643b2f25e7248fe4ae194a8698652847355400000001b8c6b0807472627cec2f77b2a33428539b64493f7f1b9f7be4dfbdee72f9aeb3b5763fa39ce5df2425d8d530698de9c1cea993bccfce6596901b6b8cb05410340000000a51a925a50a7101ca25c8165050b61421f9e54aad5b0cfb4a4947da82f5fb62fec8e96fb80bd697db33f391bf1f875179d446ac08829f85cf8fe6483ec9acf0c4000000049ea84bd8d1bf6735ac141a33a9634b3fb8e222bb03a332ccec5a5a8adfcd32653ee08d84e7069cb91425eea1dd806e69ae50246e0e8e3f13cde7660a3a12e0b",
        "fork" : "557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783",
        "confirmations" : 0
    }
}

## 跨链交易完成后，交易模板上的Token分布情况
./bigbang getbalance -a=20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3
[
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

./bigbang getbalance -a=20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3 -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
[
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]


## 查看转账后的账户情况
./bigbang getbalance
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 12.345000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y",
        "avail" : 15.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 744999985.655000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8",
        "avail" : 1927.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8",
        "avail" : 1000.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

## 测试转账是否正常
./bigbang sendfrom 1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm 1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y 3 0.1
5ce79e6559e5e953b10431bb61fadedf6ff1d884176b1b693498fc49d69a3cbd
./bigbang getbalance
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 12.345000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y",
        "avail" : 18.000000,
        "locked" : 0.000000,
        "unconfirmed" : 3.000000
    },
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 744999982.555000,
        "locked" : 0.000000,
        "unconfirmed" : 744999982.555000
    },
    {
        "address" : "20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8",
        "avail" : 1927.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8",
        "avail" : 1000.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

./bigbang getbalance -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 99999989.125000,
        "locked" : 0.000000,
        "unconfirmed" : 99999989.125000
    },
    {
        "address" : "1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 8.875000,
        "locked" : 0.000000,
        "unconfirmed" : 8.875000
    },
    {
        "address" : "20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    }
]

## 测试支链上的转账是否正常
./bigbang sendfrom 1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h 20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8 4.5 0.1 -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
5ce79ef23c0002d8592028722ffa6d21634a2fb217cffd4357a7852dfb72fae9
./bigbang getbalance -f=557384528669a894e14afe48725ef2b243f6dba3d0735b71effc21adf3bbc783
[
    {
        "address" : "1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h",
        "avail" : 99999984.525000,
        "locked" : 0.000000,
        "unconfirmed" : 99999984.525000
    },
    {
        "address" : "1wgwpv9pdwbezqz6ptv405sxhb783htn5d97278jee9stwrqsat385v1y",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm",
        "avail" : 8.875000,
        "locked" : 0.000000,
        "unconfirmed" : 8.875000
    },
    {
        "address" : "20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20r07rwj0032jssv0d3xaes1kq6z1cvjmz1jwhme0m1jf23vx48v683s3",
        "avail" : 0.000000,
        "locked" : 0.000000,
        "unconfirmed" : 0.000000
    },
    {
        "address" : "20c07j7pt2faqc7xspvz8c2qx1xp2zhsenx1379cjjd9xvrnph1kx8gj8",
        "avail" : 4.500000,
        "locked" : 0.000000,
        "unconfirmed" : 4.500000
    }
]
