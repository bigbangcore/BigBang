#!/bin/bash -v

#start up bigbang
os=`uname`
if [ "$os" == "Darwin" ]; then
  #bigbang -daemon -debug -datadir=/Users/oijen/Library/Application\ Support/Bigbang
  bigbang -daemon -datadir=/Users/oijen/Library/Application\ Support/Bigbang
else
  #bigbang -daemon -debug -datadir=/home/oijen/Bigbang
  bigbang -daemon -datadir=/home/oijen/Bigbang
fi

echo 'waiting for bigbang about 10s ...'

sleep 10

echo 'coming back!'

#import private key
prikey=9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee
bigbang importprivkey $prikey 123

#unlock imported key
pubkey=da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49
bigbang unlockkey $pubkey 123

#make fork1
#retrieve origin
genesis=`bigbang getblockhash 0 | sed -n '2,2s/"//gp' |sed -n 's/ //pg'`
echo $genesis

#retrieve latest
echo $best
best=`bigbang getblockcount`
echo $best
best=$[$best - 1]
#export best
echo $best
latest=$(bigbang getblockhash $best | sed -n '2,2s/"//gp' |sed -n 's/ //pg')
#latest=$(bigbang getblockhash 1) 
echo $latest

#d62c1fca5f2aacf9cf5738ed057d9987373508d8984734fa8fac05a6780a7cfd 

#bigbang makeorigin d62c1fca5f2aacf9cf5738ed057d9987373508d8984734fa8fac05a6780a7cfd 177cxdb2s5ewp4nwdaanc9c3nbsfxb79d22xj14a8jr27eg90bxbqh4eb 1000000.000000 'fork-1' 'FORK-1' 1.000000
owner=1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm
resMo=$(bigbang makeorigin $latest $owner 1000000.000000 'fork-1' 'FORK-1' 1.000000)
echo $resMo

#newfork=`echo $resMo | sed -n '/hash/s/[",]//gp' | awk -F: '{print $2}' | sed -n 's/ //gp'`
if [ "$os" == "Darwin" ]; then
  newfork=$(echo $resMo | awk -F\" '{print $4}')
else
  newfork=$(echo $resMo | gawk -F\" '{print $4}')
fi
echo $newfork

#data=`echo $resMo | sed -n '/hex/s/[",]//gp' | awk -F: '{print $2}' | sed -n 's/ //gp'`
if [ "$os" == "Darwin" ]; then
  data=$(echo $resMo | awk -F\" '{print $8}')
else
  data=$(echo $resMo | gawk -F\" '{print $8}')
fi
echo $data

#addr=`bigbang addnewtemplate fork '{"redeem":"177cxdb2s5ewp4nwdaanc9c3nbsfxb79d22xj14a8jr27eg90bxbqh4eb","fork":"0777d497873a9e671139dc4d33cb29be3ce4e8ae8ec592f68e3a982987c67dd5"}'`
#addrFork=$(bigbang addnewtemplate fork '{"redeem":"$owner","fork":"$newfork"}')
addrFork=$(bigbang addnewtemplate fork "{\"redeem\":\"$owner\",\"fork\":\"$newfork\"}")
echo $addrFork

#txid=$(bigbang sendfrom 177cxdb2s5ewp4nwdaanc9c3nbsfxb79d22xj14a8jr27eg90bxbqh4eb $addr 1000000.000000 0.07 -f=d62c1fca5f2aacf9cf5738ed057d9987373508d8984734fa8fac05a6780a7cfd -d=010000ffc06f585a52e884248f9ad14fc5db499768f05c7bccc6cc20d342f3d77095c0e42ba877e60000000000000000000000000000000000000000000000000000000000000000728001000000e106666f726b2d31e206464f524b2d314301a46400000000000000a540420f0000000000e6210139d9d6ac592bb962578d52aac4b0755e5fd59d2d10bb20914896047741205f57c752e884248f9ad14fc5db499768f05c7bccc6cc20d342f3d77095c0e42ba877e6880000000001000001c06f585a000000000000000000000000000000000000000000000000000000000000000000000000000139d9d6ac592bb962578d52aac4b0755e5fd59d2d10bb20914896047741205f570010a5d4e8000000000000000000000006666f726b2d31000040d325e82e4d99e9b0d4b1e16858a509991ab5d8201601b8d5079a1510509693d40114337fb339f08eb2262f29f1a3279393a50911fcdf518c59cc78e39fb8b005)
#txid=$(bigbang sendfrom $owner $addr 1000000.000000 0.07 -f=d62c1fca5f2aacf9cf5738ed057d9987373508d8984734fa8fac05a6780a7cfd -d=010000ffc06f585afd7c0a78a605ac8ffa344798d808353787997d05ed3857cff9ac2a5fca1f2cd6000000000000000000000000000000000000000000000000000000000000000070600100e106666f726b2d31e206464f524b2d314301a46400000000000000a540420f0000000000e6210139d9d6ac592bb962578d52aac4b0755e5fd59d2d10bb20914896047741205f57c7fd7c0a78a605ac8ffa344798d808353787997d05ed3857cff9ac2a5fca1f2cd6880000000001000001c06f585a000000000000000000000000000000000000000000000000000000000000000000000000000139d9d6ac592bb962578d52aac4b0755e5fd59d2d10bb20914896047741205f570010a5d4e8000000000000000000000006666f726b2d310000406949c658e59fdf0c6f19fb9bf61d8843877bc2c84889fe8eb9d54436a6091038b9a2a095f243287055ddd9018f2f17950aace52ca557026714f38035295a5006)
txid=$(bigbang sendfrom $owner $addrFork 1000000.000000 0.07 -f=$genesis -d=$data)
echo $txid


bigbang getblockcount
bigbang getbalance

#sleep 1000


sleep 60

for (( i=1; i <= 2; i++ ))
do
#  bigbang gettransaction 5c861df327b17cd1ef3b882e9d1b295152b45236b292f1f19d0813c566fc9baf
#  bigbang gettransaction 5c862a30927101368afaaad7b96685dbf613b0bd296c67796560a119fd394076
  bigbang gettransaction $txid

  sleep 30
done

bigbang getblockcount
bigbang getbalance

sleep 5

bigbang listfork

bigbang getblockcount

bigbang getbalance

#bigbang getbalance -f=d62c1fca5f2aacf9cf5738ed057d9987373508d8984734fa8fac05a6780a7cfd -a=177cxdb2s5ewp4nwdaanc9c3nbsfxb79d22xj14a8jr27eg90bxbqh4eb
bigbang getbalance -f=$genesis -a=1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm

bigbang getbalance -f=$newfork -a=1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm

bigbang getbalance -f=$newfork -a=$addrFork

for (( ; ; ))
do
  bigbang getblockcount
  bigbang getbalance
  sleep 120
done

