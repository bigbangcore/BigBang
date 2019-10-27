#!/bin/bash

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

runonce=1
if [ "$runonce" -eq 1 ]; then
    # import sender
    ret=`bigbang importprivkey f43fab79c29ebb94cc680c97d5505c0c95842f0fa345d08e2839a85f60e6e38a 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile
    # import receiver
    ret=`bigbang importprivkey 20e6078147f2d4ee757b71584f64d088519fd4a425780bdb630d45bc89c8fe1e 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile
    # import genesis
    ret=`bigbang importprivkey 9df809804369829983150491d1086b99f6493356f91ccc080e661a76a976a4ee 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile

    # charge to sender from genesis
    ret=`bigbang unlockkey da915f7d9e1b1f6ed99fd816ff977a7d1f17cc95ba0209eef770fb9d00638b49 123`
	echo $(date -Iseconds)     $ret | tee -a $logfile
    ret=`bigbang sendfrom 1965p604xzdrffvg90ax9bk0q3xyqn5zz2vc9zpbe3wdswzazj7d144mm 1erazk3ww4nap6bm46mfct98axe2phsqcba00987ezqqdpswfv6yexz7a 10000000.00 11.26`
	echo $(date -Iseconds)     $ret | tee -a $logfile
fi

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
# source address
#"privkey" : "f43fab79c29ebb94cc680c97d5505c0c95842f0fa345d08e2839a85f60e6e38a",
#"pubkey" : "bcd98f67dbeefdeea004805aece66885eb0a25cd1e35842e6355259c8ff91576"
#"address" : "1erazk3ww4nap6bm46mfct98axe2phsqcba00987ezqqdpswfv6yexz7a",
# target address
#"privkey" : "20e6078147f2d4ee757b71584f64d088519fd4a425780bdb630d45bc89c8fe1e",
#"pubkey" : "9513efbd24eddf2cb70f7bca1b23cd692b4c6af5bf6dbe92bda5b3cccf61498d"
#"address" : "1hn4p3kycpejvv4nydpzzatjc5dmwt8rvs9xgzdscvzpj9fff2eaw8y6j",
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
  # unlock key
  local paykey="bcd98f67dbeefdeea004805aece66885eb0a25cd1e35842e6355259c8ff91576"
  local pwd=123
  unlockkey $paykey $pwd
  if [ "$?" -ne 0 ]
  then
    return -1
  fi

  local payer="1erazk3ww4nap6bm46mfct98axe2phsqcba00987ezqqdpswfv6yexz7a"
  local payee="1hn4p3kycpejvv4nydpzzatjc5dmwt8rvs9xgzdscvzpj9fff2eaw8y6j"
  local amount=0.000100
  local fee=0.000100
  local res
  local rpccmd="$app sendfrom $payer $payee $amount $fee"
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
