#!/bin/bash

echo $#
if [ "$#" -ne 1 ]
then
	echo "expect one argument of which one? 'm' for multiverse or 'b' for bigbang, please try again."
	exit -1
elif [ "$#" -eq 1 ]
then
# version of corewallet
	if [ "$1" = "m" ]; then
		app="multiverse"
		dir="multiverse"
	elif [ "$1" = "b" ]; then
		app="bigbang"
		dir="bigbang"
	else
		echo "there is no parameter like this, please try again."
		exit -1
	fi
fi

ln=$(ps aux | grep "$app" | grep -v "console" | grep -v "grep" | grep -v ".log" | wc -l)
if [ "$ln" -ge 1 ]; then
	echo "$app has been running, exit..."
	exit -3
fi

$app -daemon -debug -datadir=$HOME/.$dir

echo 'prepare to sleep 10s...'

sleep 10

echo 'wake up!'

if [ $app = "multiverse" ]; then
	#PubKey : 575f2041770496489120bb102d9dd55f5e75b0c4aa528d5762b92b59acd6d939
	#Secret : bf0ebca9be985104e88b6a24e54cedbcdd1696a8984c8fdd7bc96917efb5a1ed

	#PubKey : 749b1fe6ad43c24bd8bff20a222ef74fdf0763a7efa0761619b99ec73985016c
	#Secret : 05f07d1e09b60b74538a1e021c98956a7c508fa35f6c3eba01c9ab1f6a871611

	#PubKey : 6236d780f9f743707d57b3feb19d21c8a867577f5e83c163774222bb7ef8d8cb
	#Secret : 7c6a6aba05cec77a998c19649ee1fa0e29c7b5246d0e3a6501ee1d4d81dd73ea

	privkey="bf0ebca9be985104e88b6a24e54cedbcdd1696a8984c8fdd7bc96917efb5a1ed"
	pubkey="749b1fe6ad43c24bd8bff20a222ef74fdf0763a7efa0761619b99ec73985016c"
	owner="177cxdb2s5ewp4nwdaanc9c3nbsfxb79d22xj14a8jr27eg90bxbqh4eb"
	
	# before 2.15
	#mainfork="e677a82be4c09570d7f342d320ccc6cc7b5cf0689749dbc54fd19a8f2484e852"
	# before 4.1
	mainfork="d62c1fca5f2aacf9cf5738ed057d9987373508d8984734fa8fac05a6780a7cfd"
elif [ $app = "bigbang" ]; then
	#PubKey : da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49
	#Secret : 9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee

	#PubKey : e76226a3933711f195aa6c1879e2381976b33337bf7b3b296edd8dff105b24b5
	#Secret : c00f1c287f0d9c0931b1b3f540409e8f7ad362427df4a75286992cb9200096b1

	#PubKey : fe8455584d820639d140dad74d2644d742616ae2433e61c0423ec350c2226b78
	#Secret : 9f1e445c2a8e74fabbb7c53e31323b2316112990078cbd8d27b2cd7100a1648d
	
	privkey="9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee"
	pubkey="e76226a3933711f195aa6c1879e2381976b33337bf7b3b296edd8dff105b24b5"
	owner="1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm"

	mainfork="92099485ffec67128fe4dbaca96ed8faf54ccc0b4760cd0d78a3d1e051a2498f"
fi
echo
echo "privkey=$privkey"
echo "pubkey=$pubkey"
echo "owner=$owner"
echo "mainfork=$mainfork"

#import private key
pk=`$app importprivkey $privkey 123456`
#unlock imported key
$app unlockkey $pk 123456

forktx1=""
forktx2=""
newfork1=""
newfork2=""
# make fork
function makefork
{
	if [ $# -ne 1 ]; then
		echo "you should pass one argument to this function."
		return -1
	fi
	local forkno=$1
	#retrieve origin
	genesis=`$app getblockhash 0 | sed -n '2,2s/"//gp' |sed -n 's/ //pg'`
	echo "genesis=$genesis"

	#retrieve latest
	local best=`$app getblockcount`
	echo "getblockcount=$best"
	local best=$[$best - 1]
	#export best
	echo "bestHeight=$best"
	local latest=$($app getblockhash $best | sed -n '2,2s/"//gp' |sed -n 's/ //pg')
	#latest=$(bigbang getblockhash 1) 
	echo "latest block hash=$latest"

	local prevblock=$mainfork
	local forkbytime=$(date +"%Y-%m-%d-%H-%M")
	local forksymbol="SG2-"$forkno
	local resMo=$($app makeorigin $latest $owner 1000000.000000 $forkbytime $forksymbol 1.000000)
	echo "makeorigin-result=$resMo"

	#newfork=`echo $resMo | sed -n '/hash/s/[",]//gp' | awk -F: '{print $2}' | sed -n 's/ //gp'`
	#data=`echo $resMo | sed -n '/hex/s/[",]//gp' | awk -F: '{print $2}' | sed -n 's/ //gp'`
	if [ "$os" == "Darwin" ]; then
		local newfork=$(echo $resMo | awk -F\" '{print $4}')
		local data=$(echo $resMo | awk -F\" '{print $8}')
	else
		local newfork=$(echo $resMo | gawk -F\" '{print $4}')
		local data=$(echo $resMo | gawk -F\" '{print $8}')
	fi
	echo
	echo "newfork-hash=$newfork"
	echo "data=$data"

	local addrFork=$($app addnewtemplate fork "{\"redeem\":\"$owner\",\"fork\":\"$newfork\"}")
	echo
	echo "add-fork-template-result=$addrFork"

	local txfork=$($app sendfrom $owner $addrFork 1000000.000000 0.07 -f=$genesis -d=$data)
	echo
	echo "fork-tx-hash=$txfork"
	if [ "$1" -eq 1 ]; then
		forktx1=$txfork
		newfork1=$newfork
	else
		forktx2=$txfork
		newfork2=$newfork
	fi
}

makefork 1 || return -1
sleep 120
makefork 2 || return -1

# make dpos
addrDelegate=$($app addnewtemplate delegate "{\"delegate\":\"$pubkey\",\"owner\":\"$owner\"}")
echo
echo "add-dpos-template-result=$addrDelegate"
txdpos=`$app sendfrom $owner $addrDelegate 500000000.000000 7.000000 -f=$genesis`
echo "dpos-tx-hash=$txdpos"

sleep 10
# make pow
addrMint=$($app addnewtemplate mint "{\"mint\":\"$pubkey\",\"spent\":\"$owner\"}")
echo
echo "add-pow-template-result=$addrMint"

sleep 60

for (( i=1; i <= 2; i++ ))
do
  $app gettransaction $forktx1

  $app gettransaction $forktx2

  $app gettransaction $txdpos
  sleep 30
done

sleep 5

$app listfork

$app getbalance -f=$newfork1 -a=$owner

$app getbalance -f=$newfork2 -a=$owner

$app getbalance -f=$genesis -a=$owner

$app getbalance -f=$newfork1

$app getbalance -f=$newfork2

$app getbalance -f=$genesis




