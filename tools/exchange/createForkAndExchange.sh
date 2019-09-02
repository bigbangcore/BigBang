#!/bin/sh
bigbang stop
echo "# ps aux| grep bigbang"
ps aux| grep bigbang
echo "# pkill bigbang"
pkill -9 bigbang
echo "# ps aux| grep bigbang"
ps aux| grep bigbang

sudo rm -rf /home/shang/.bigbang
bigbang -daemon
sleep 1
OS=`uname`

echo "\n"
echo "# bigbang importprivkey prikeys 123456"
prikeyA=8102db22dcc095d4d685613ca89a5d938e2de39954b5aa46f975ded23d9c9d69
pubkeyA=$(bigbang importprivkey $prikeyA 123456)

prikeyB=27ee1dce5fe525a38ffb3ee1515206451fde7443e422cc9a26ae0d65766eb608
pubkeyB=$(bigbang importprivkey $prikeyB 123456)

prikeyC=1f6c18ed562d9585de0a43cdefc551bdd1b82878cbb11c3b91209d0eff985b73
pubkeyC=$(bigbang importprivkey $prikeyC 123456)

prikeyD=4414d207483bee46fc116b57925efa26e5313782af19a9ba34c9d805b8ee25d2
pubkeyD=$(bigbang importprivkey $prikeyD 123456)

prikeyE=9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee
pubkeyE=$(bigbang importprivkey $prikeyE 123456)

prikeyF=bf0ebca9be985104e88b6a24e54cedbcdd1696a8984c8fdd7bc96917efb5a1ed
pubkeyF=$(bigbang importprivkey $prikeyF 123456)

echo prikeyA=$prikeyA
echo pubkeyA=$pubkeyA
echo prikeyB=$prikeyB
echo pubkeyB=$pubkeyB
echo prikeyC=$prikeyC
echo pubkeyC=$pubkeyC
echo prikeyD=$prikeyD
echo pubkeyD=$pubkeyD
echo prikeyE=$prikeyE
echo pubkeyE=$pubkeyE
echo prikeyF=$prikeyF
echo pubkeyF=$pubkeyF

echo "\n"
echo "# bigbang unlockkey pubkeys 123456"
bigbang unlockkey $pubkeyA 123456
bigbang unlockkey $pubkeyB 123456
bigbang unlockkey $pubkeyC 123456
bigbang unlockkey $pubkeyD 123456
bigbang unlockkey $pubkeyE 123456
bigbang unlockkey $pubkeyF 123456

sleep 1
echo "\n"

#wallet address
echo "# get wallet address by pubkey---bigbang getpubkeyaddress pubkey"
addrA=$(bigbang getpubkeyaddress $pubkeyA)
echo addrA=$addrA
addrB=$(bigbang getpubkeyaddress $pubkeyB)
echo addrB=$addrB
addrC=$(bigbang getpubkeyaddress $pubkeyC)
echo addrC=$addrC
addrD=$(bigbang getpubkeyaddress $pubkeyD)
echo addrD=$addrD
addrE=$(bigbang getpubkeyaddress $pubkeyE)
echo addrE=$addrE
addrF=$(bigbang getpubkeyaddress $pubkeyF)
echo addrF=$addrF

sleep 1
echo "\n"

echo "# bigbang addnewtemplate mint \"{\"mint\":\"$pubkeyA\",\"spent\":\"$addrA\"}\""
tmpAddrA=$(bigbang addnewtemplate mint "{\"mint\":\"$pubkeyA\",\"spent\":\"$addrA\"}")
echo tmpAddrA=$tmpAddrA

echo "\n"
echo "# bigbang addnewtemplate mint \"{\"mint\":\"$pubkeyB\",\"spent\":\"$addrB\"}\""
tmpAddrB=$(bigbang addnewtemplate mint "{\"mint\":\"$pubkeyB\",\"spent\":\"$addrB\"}")
echo tmpAddrB=$tmpAddrB

echo "\n"
echo "# bigbang addnewtemplate mint \"{\"mint\":\"$pubkeyC\",\"spent\":\"$addrC\"}\""
tmpAddrC=$(bigbang addnewtemplate mint "{\"mint\":\"$pubkeyC\",\"spent\":\"$addrC\"}")
echo tmpAddrC=$tmpAddrC

sleep 2
echo "\n"

#bigbang miner <wallet addr> <prikey>）
gnome-terminal --title="A" -x bash -c "bigbang miner $addrA $prikeyA;exec bash;"
#bigbang miner 20g02cqqm3hq4txc78b5taeh8x4g88f3vg7t13g2zq232yfz340p0vzh8 8102db22dcc095d4d685613ca89a5d938e2de39954b5aa46f975ded23d9c9d69

echo "\n"
echo "# Wait 120 seconds for miners to produce two blocks ..."
sleep 120
echo "\n"

echo "# bigbang getblockhash 0"
forkMain_hash=$(bigbang getblockhash 0 | sed -n '2,2s/"//gp' |sed -n 's/ //pg')
echo forkMain_hash=$forkMain_hash

echo "# bigbang getblockhash 1"
forkA_hash=$(bigbang getblockhash 1 | sed -n '2,2s/"//gp' |sed -n 's/ //pg')
echo forkA_hash=$forkA_hash

echo "\n"
echo "# bigbang getblockhash 2"
forkB_hash=$(bigbang getblockhash 2 | sed -n '2,2s/"//gp' |sed -n 's/ //pg')
echo forkB_hash=$forkB_hash

echo "\n"
echo "# make origin BigBangA ... \n bigbang makeorigin $forkA_hash $addrA 1000000000 \"BigBangA\" \"BA\" 15"
resMoA=$(bigbang makeorigin $forkA_hash $addrA 1000000000 "BigBangA" "BA" 15)
echo resMoA=$resMoA

echo "\n"
echo "# make origin BigBangB ... \n bigbang makeorigin $forkB_hash $addrB 1000000000 \"BigBangB\" \"BB\" 1.000000"
resMoB=$(bigbang makeorigin $forkB_hash $addrB 1000000000 'BigBangB' 'BB' 1.000000)
echo resMoB=$resMoB

echo "\n"
newforkA=$(echo $resMoA | awk -F\" '{print $4}')
echo newforkA=$newforkA

echo "\n"
newforkB=$(echo $resMoB | awk -F\" '{print $4}')
echo newforkB=$newforkB

echo "\n"
dataA=$(echo $resMoA | awk -F\" '{print $8}')
echo dataA=$dataA

echo "\n"
dataB=$(echo $resMoB | awk -F\" '{print $8}')
echo dataB=$dataB

echo "\n"
#fork address
addrForkA=$(bigbang addnewtemplate fork "{\"redeem\":\"$addrA\",\"fork\":\"$newforkA\"}")
echo addrForkA=$addrForkA

echo "\n"
addrForkB=$(bigbang addnewtemplate fork "{\"redeem\":\"$addrB\",\"fork\":\"$newforkB\"}")
echo addrForkB=$addrForkB

echo "\n"
echo "# bigbang sendfrom $addrE $tmpAddrA 2000 2.5"
echo $(bigbang sendfrom $addrE $tmpAddrA 2000 2.5)

echo "\n"
sleep 2
echo "# bigbang sendfrom $addrE $tmpAddrB 2000 3.5"
echo $(bigbang sendfrom $addrE $tmpAddrB 2000 3.5)

echo "\n"
echo "# bigbang getbalance ..."
bigbang getbalance

echo "\n"
echo "# wait for 60 seconds ..."
sleep 60

echo "\n"
echo "# bigbang getbalance ..."
bigbang getbalance

echo "\n"
echo "# bigbang sendfrom $tmpAddrA $addrForkA 1000 5 -d=$dataA"
echo $(bigbang sendfrom $tmpAddrA $addrForkA 1000 5 -d=$dataA)

echo "\n"
echo "# bigbang getbalance ..."
bigbang getbalance

echo "\n"
bigbang stop

#echo "\n"
#echo "# ps aux| grep bigbang"
#ps aux| grep bigbang
#echo "# pkill bigbang"
#pkill -9 bigbang
#echo "# ps aux| grep bigbang"
#ps aux| grep bigbang
#sleep 2

echo "\n"
echo "# wait for 10 seconds ..."
sleep 10

echo "\n"
echo "# bigbang -addfork=$newforkA -daemon"
bigbang -addfork=$newforkA -daemon

sleep 60
echo "# wait for 60 seconds ..."

echo "\n"
echo "# bigbang getblockcount"
bigbang getblockcount
echo "\n"
echo "# bigbang listfork"
bigbang listfork

echo "\n"
echo "# bigbang getbalance ..."
bigbang getbalance

echo "\n"
echo "# bigbang getbalance -a=$addrE -f=$forkMain_hash"
bigbang getbalance -a=$addrE -f=$forkMain_hash

echo "\n"
echo "# bigbang getbalance -f=$newforkA"
bigbang getbalance -f=$newforkA

echo "\n"
echo "# bigbang getbalance -a=$addrA -f=$newforkA"
bigbang getbalance -a=$addrA -f=$newforkA

sleep 5
gnome-terminal --title="B" -x bash -c "bigbang miner $addrB $prikeyB;exec bash;"
echo "\n"
echo "# wait for 5 seconds ..."
sleep 5

echo "\n"
bigbang stop

#echo "\n"
#echo "# ps aux| grep bigbang"
#ps aux| grep bigbang
#echo "# pkill bigbang"
#pkill -9 bigbang
#echo "# ps aux| grep bigbang"
#ps aux| grep bigbang

sleep 6
echo "sleep for 6 seconds ..."
echo "\n"
echo "# bigbang -addfork=$newforkB -daemon"
bigbang -addfork=$newforkB -daemon

sleep 5
gnome-terminal --title="C" -x bash -c "bigbang miner $addrC $prikeyC;exec bash;"
echo "\n"
echo "# wait for 120 seconds ..."
sleep 120

for (( i=1; i <= 5; i++ ))
do
    echo "\n"
    echo "# bigbang getblockcount"
    bigbang getblockcount

    echo "\n"
    echo "# bigbang listfork"
    bigbang listfork

    echo "\n"
    echo "# bigbang getbalance ..."
    bigbang getbalance

    echo "\n"
    echo "# bigbang getbalance -a=$addrE -f=$forkMain_hash"
    bigbang getbalance -a=$addrE -f=$forkMain_hash

    echo "\n"
    echo "# bigbang getbalance -f=$newforkA"
    bigbang getbalance -f=$newforkA

    echo "\n"
    echo "# bigbang getbalance -f=$newforkB"
    bigbang getbalance -f=$newforkB

    echo "\n"
    echo "# bigbang getbalance -a=$addrA -f=$newforkA"
    bigbang getbalance -a=$addrA -f=$newforkA

    echo "\n"
    echo "# bigbang getbalance -a=$addrB -f=$newforkB"
    bigbang getbalance -a=$addrB -f=$newforkB
    sleep 60
done

echo "\n"
echo "/////////////////////////////////////////////"
echo "//                EXCHANGE                 //"
echo "/////////////////////////////////////////////"

echo "\n"
echo "# The next step is to do cross-chain transactions ..."
echo "# main chain:$forkMain_hash,and subchain:$newforkA,we use 100  main chain tokens to exchange with 200 subchain tokens ..."
#main:92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f
#subc:c3c64342d00c118b20081a22e628aba8fad72bf0da9c16506fbbb6214b220233

echo "\n"
echo "# bigbang unlockkey pubkeys 123456"
bigbang unlockkey $pubkeyA 123456
bigbang unlockkey $pubkeyB 123456
bigbang unlockkey $pubkeyC 123456
bigbang unlockkey $pubkeyD 123456
bigbang unlockkey $pubkeyE 123456
bigbang unlockkey $pubkeyF 123456

echo "\n"
echo "# bigbang listkey"
bigbang listkey

echo "\n"
echo "# 1--main chain sends 100 tokens to himself,and lock them until 100 blocks later,the param -d is the object who will exchange with ..."

if [ -z "$forkMain_hash" ]
then
    echo "\n"
    echo "Main chain is empty ..."
    bigbang stop
fi

if [ -z "$newforkA" ]
then
    echo "\n"
    echo "Subchain is empty ..."
    bigbang stop
fi

if [ -z "$addrE" ]
then
    echo "\n"
    echo "Main chain wallet address is empty ..."
    bigbang stop
fi

if [ -z "$addrA" ]
then
    echo "\n"
    echo "Subchain wallet address is empty ..."
    bigbang stop
fi

echo "\n"
m_fork=$forkMain_hash
s_fork=$newforkA
m_addr=$addrE
s_addr=$addrA
m_free=1.0000
s_free=1.0000

echo "\n"
echo "# bigbang sendfrom $m_addr $m_addr 100 1 -f=$m_fork -d=$s_addr -l=100"
m_hash=$(bigbang sendfrom $m_addr $m_addr 100 1 -f=$m_fork -d=$s_addr -l=100)
#bigbang sendfrom 1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm 1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm 100 1 -f=92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f -d=1w8ehkb2jc0qcn7wze3tv8enzzwmytn9b7n7gghwfa219rv1vhhd82n6h -l=100

sleep 1
echo "\n"
echo "# bigbang gettransaction $m_hash"
bigbang gettransaction $m_hash

sleep 1
echo "\n"
echo "# 2--subchain sends 200 tokens to himself,and lock them until 50 blocks later,the param -d is the object who will exchange with ..."
echo "# bigbang sendfrom $s_addr $s_addr 200 1 -f=$s_fork -d=$m_addr -l=50"
s_hash=$(bigbang sendfrom $s_addr $s_addr 200 1 -f=$s_fork -d=$m_addr -l=50)

echo "\n"
echo "# bigbang gettransaction $s_hash"
bigbang gettransaction $s_hash

echo "\n"
echo "# 3--Verification:bigbang sendfrom $m_addr $addrC 10 1,we can find that the locked tokens isn't decrease but the total tokens is 10 fewer ..."
bigbang sendfrom $m_addr $addrC 10 1

echo m_hash=$m_hash
echo s_hash=$s_hash
echo m_fork=$m_fork
echo s_fork=$s_fork
echo m_addr=$m_addr
echo s_addr=$s_addr
echo m_free=1.0000
echo s_free=1.0000


# ./BigBang exchange m_hash s_hash m_fork s_fork m_addr s_addr m_free s_free m_sign/s_sign
echo "\n"
echo "# 4--The user on main chain signs transaction data first ..."
echo "# bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_sign"
m_signRe=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_sign)
echo m_signRe=$m_signRe

echo "\n"
echo "# 5--The user on subchain signs transaction data then ..."
echo "# bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_sign"
s_signRe=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_sign)
echo s_signRe=$s_signRe

echo "\n"
echo "# 6--The user on main chain verify that the signature is valid ..."
echo "# bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_verify -m=$m_signRe -s=$s_signRe"
m_verify_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_verify -m=$m_signRe -s=$s_signRe)
echo m_verify_tra=$m_verify_tra


echo "\n"
echo "# 7--The user on subchain verify that the signature is valid ..."
echo "# bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_verify -m=$m_signRe -s=$s_signRe"
s_verify_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_verify -m=$m_signRe -s=$s_signRe)
echo s_verify_tra=$s_verify_tra

echo "\n"
echo "# 8--Both sides of the transaction start broadcasting information, and the broadcasting process requires that whoever locks fewer data blocks than the other needs to send the signature results to the other party first. Otherwise, the security of the transaction cannot be guaranteed."
echo "# bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_submit -m=$m_signRe -s=$s_signRe"
m_submit_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free m_submit -m=$m_signRe -s=$s_signRe)
echo m_submit_tra=$m_submit_tra
echo "\n"
bigbang gettransaction $m_submit_tra

echo "\n"
echo "# 9--bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_submit -m=$m_signRe -s=$s_signRe"
s_submit_tra=$(bigbang exchange $m_hash $s_hash $m_fork $s_fork $m_addr $s_addr $m_free $s_free s_submit -m=$m_signRe -s=$s_signRe)
echo s_submit_tra=$s_submit_tra
echo "\n"
echo "# bigbang gettransaction $s_submit_tra"
bigbang gettransaction $s_submit_tra

echo "\n"
echo "# bigbang getbalance -a=$m_addr -f=$m_fork"
bigbang getbalance -a=$m_addr -f=$m_fork

echo "\n"
echo "# bigbang getbalance -a=$s_addr -f=$s_fork"
bigbang getbalance -a=$s_addr -f=$s_fork

