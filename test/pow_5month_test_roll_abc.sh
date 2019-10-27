#!/bin/bash

if [ "$#" -ne 1 ]
then
	echo "expect only one argument of which: a or b or c, please try again."
	exit -1
fi

# tx direction
if [ "$1" = "a" ]; then
    # from A to B
	fromwhich=$1
elif [ "$1" = "b" ]; then
	# from B to C
	fromwhich=$1
elif [ "$1" = "c" ]; then
	# from C to A
	fromwhich=$1
else
	echo "there is no parameter matching expected, please try again."
	exit -1
fi

app="bigbang"

logfile="./txtest.log"
if [ ! -f "$logfile" ]; then
  touch $logfile
fi

# check if bigbang has run right now, otherwise start it up.
instance=$(ps aux | grep "$app" | grep -v "console" | grep -v "grep" | grep -v ".log" | wc -l)
if [ "$instance" -le 1 ]; then
    $app -daemon -debug
	echo $(date -Iseconds)     bigbang has started. | tee -a $logfile
    echo 'Waiting for bigbang daemon running up...' | tee -a $logfile
    sleep 10
fi

#cryptonightaddress=1erazk3ww4nap6bm46mfct98axe2phsqcba00987ezqqdpswfv6yexz7a
#cryptonightkey=20e6078147f2d4ee757b71584f64d088519fd4a425780bdb630d45bc89c8fe1e
runonce=1
if [ "$runonce" -eq 1 ]; then
    # import address A
    ret=`bigbang importprivkey fa7390d1bf9fff170827d5eca4bde011f56064ac6a63e3ab41872a0ab5afcf6b 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile
    # import address B
    ret=`bigbang importprivkey 7ff746e5ea507937ad4660b72b664bdc4a71909691638e78db596513f713c6fb 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile
    # import address C
    ret=`bigbang importprivkey 992766ac5c6d961623188d947adbe594d6b89abe1cd32b301d4b2139d5c40dc5 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile

    # import mint template address
    ret=`bigbang addnewtemplate mint '{"mint": "9513efbd24eddf2cb70f7bca1b23cd692b4c6af5bf6dbe92bda5b3cccf61498d", "spent": "1erazk3ww4nap6bm46mfct98axe2phsqcba00987ezqqdpswfv6yexz7a"}'`
	echo $(date -Iseconds)     $ret | tee -a $logfile
    # import mint spend address
    ret=`bigbang importprivkey f43fab79c29ebb94cc680c97d5505c0c95842f0fa345d08e2839a85f60e6e38a 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile
fi

# spend address
#"privkey" : "f43fab79c29ebb94cc680c97d5505c0c95842f0fa345d08e2839a85f60e6e38a",
#"pubkey" : "bcd98f67dbeefdeea004805aece66885eb0a25cd1e35842e6355259c8ff91576"
#"address" : "1erazk3ww4nap6bm46mfct98axe2phsqcba00987ezqqdpswfv6yexz7a",
# mint address
#"privkey" : "20e6078147f2d4ee757b71584f64d088519fd4a425780bdb630d45bc89c8fe1e",
#"pubkey" : "9513efbd24eddf2cb70f7bca1b23cd692b4c6af5bf6dbe92bda5b3cccf61498d"
#"address" : "1hn4p3kycpejvv4nydpzzatjc5dmwt8rvs9xgzdscvzpj9fff2eaw8y6j",
# mint template address
# 20g0bdg20sx9ajssd39ghzbrjtphqcb0q1ng40skeahh2pa6xghbnxgw1
function charge
{

    # unlock spend key
    ret=`bigbang unlockkey bcd98f67dbeefdeea004805aece66885eb0a25cd1e35842e6355259c8ff91576 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile
	# charge A from mint
    ret=`bigbang sendfrom 20g0bdg20sx9ajssd39ghzbrjtphqcb0q1ng40skeahh2pa6xghbnxgw1 123bd9kvh4gze86yr8d1gqscebx0zs4z8wc270py227h8cres28qf4xeg 1100.00 3.0`
	echo $(date -Iseconds)     $ret | tee -a $logfile
}

function unlockkey()
{
  echo | tee -a $logfile
  echo =================================== | tee -a $logfile
  echo "entering  unlockkey() at" `date -R`| tee -a $logfile
  echo =================================== | tee -a $logfile
  echo | tee -a $logfile
  if [ $# -ne 2 ]; then
    echo "need 2 arguments: key and phrase"
	return -1
  fi
  local key=$1
  local pwd=$2
  local rpccmd="$app unlockkey $key $pwd"
  echo $(date -Iseconds)     $rpccmd | tee -a $logfile
  local res=`$rpccmd`
  echo $(date -Iseconds)     $res | tee -a $logfile
  return 0
}

# input: interval(unit: second) - between 2 transactions generation, default is 3 seconds
# address A
#"privkey" : "fa7390d1bf9fff170827d5eca4bde011f56064ac6a63e3ab41872a0ab5afcf6b",
#"pubkey" : "2e12d96186e211c25b7004e3e893fc415f8ee50b4343d81be43e2471cfd4d610"
#"address" : "123bd9kvh4gze86yr8d1gqscebx0zs4z8wc270py227h8cres28qf4xeg"

# address B
#"privkey" : "7ff746e5ea507937ad4660b72b664bdc4a71909691638e78db596513f713c6fb",
#"pubkey" : "42d3a6bc5e9cadf47f4d8cac85f917c38de149ea4e663c011acf83139ef1eb0a"
#"address" : "11bnz37gkgf7hm09wcs7emjf1hq1hfyc5nj64tzzmnpe5xf56td1chy5q"

# address C
#"privkey" : "992766ac5c6d961623188d947adbe594d6b89abe1cd32b301d4b2139d5c40dc5",
#"pubkey" : "ac81e859f4038c14430bbc20b016bd00975940d1615977facf466d5762c87a8f"
#"address" : "1hxxcgrjqdn3czykqb5gx2g2sjw0bt5ng42y0pgrmhg1z8pf8g6pfmzz5",
sendfrom()
{
  echo | tee -a $logfile
  echo =================================== | tee -a $logfile
  echo "entering sendfrom() at" `date -R`  | tee -a $logfile
  echo =================================== | tee -a $logfile
  echo | tee -a $logfile
  local gap=3  #unit: second
  if [ "$#" -eq 1 ]; then
    gap=$1
  fi

  case $fromwhich in
  a)
      local paykey="2e12d96186e211c25b7004e3e893fc415f8ee50b4343d81be43e2471cfd4d610"
      local pwd=123
      local payer="123bd9kvh4gze86yr8d1gqscebx0zs4z8wc270py227h8cres28qf4xeg"
      local payee="11bnz37gkgf7hm09wcs7emjf1hq1hfyc5nj64tzzmnpe5xf56td1chy5q"
      echo "wait for around 60s to receive charge..."
      sleep 67
      charge
      ;;
  b)
      local paykey="42d3a6bc5e9cadf47f4d8cac85f917c38de149ea4e663c011acf83139ef1eb0a"
      local pwd=123
      local payer="11bnz37gkgf7hm09wcs7emjf1hq1hfyc5nj64tzzmnpe5xf56td1chy5q"
      local payee="1hxxcgrjqdn3czykqb5gx2g2sjw0bt5ng42y0pgrmhg1z8pf8g6pfmzz5"
      ;;
  c)
      local paykey="ac81e859f4038c14430bbc20b016bd00975940d1615977facf466d5762c87a8f"
      local pwd=123
      local payer="1hxxcgrjqdn3czykqb5gx2g2sjw0bt5ng42y0pgrmhg1z8pf8g6pfmzz5"
      local payee="123bd9kvh4gze86yr8d1gqscebx0zs4z8wc270py227h8cres28qf4xeg"
      ;;
  *)
      echo 'should not be here!'
      ;;
  esac

  # unlock key
  unlockkey $paykey $pwd
  if [ "$?" -ne 0 ]
  then
    return -1
  fi

  local amount=0.000100
  local fee=0.000100
  local res
  local rpccmd="$app sendfrom $payer $payee $amount $fee"
  local acc=0
  for((;;))
  do
    res=`$rpccmd`
	echo $(date -Iseconds)     $rpccmd | tee -a $logfile
	if [ $? -ne 0 ]; then
		echo $?
		echo "Executing sendfrom command failed!"
		break
	fi
	echo $(date -Iseconds)     'tx hash is' $res | tee -a $logfile
	echo
	echo
	echo
	res=`bigbang gettxpool`
	echo $(date -Iseconds)     'tx pool statistics:' $res | tee -a $logfile
	sleep $gap
	acc=$[$acc+$gap]
	if [ $acc -ge 60 ]; then
	  charge
	  acc=$[$acc-60]
	fi
  done
}

function __main_test__
{
  echo | tee -a $logfile
  echo =================================== | tee -a $logfile
  echo "entering maintest() at" `date -R`  | tee -a $logfile
  echo =================================== | tee -a $logfile
  echo | tee -a $logfile
  for((;;))
  do
    sendfrom 1
  done
}



 __main_test__
