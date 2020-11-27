function setcode {
    cd /opt/src/MGP-Contracts && cleos set contract $con ./build/contracts/mgp.bpvoting/
    cleos set account permission $con active '{"threshold": 1,"keys": [{"key": "'$pubkey'","weight": 1}],"accounts": [{"permission":{"actor":"'$con'","permission":"eosio.code"},"weight":1}]}' owner -p $con

}

function init {
  echo "0. conf & init"
  echo ""
  cleos push action $con config '[20,30,50,21,180,600,1,"100.0000 MGP","200.0000 MGP","10.0000 MGP"]' -p $con
  cleos push action $con init '[]' -p $con
  cleos push action $con setelect '[1,0]' -p $con 
}

function reward {
  echo "1. send rewards (300)"
  echo ""
  cleos transfer eosio $con "300.0000 MGP"
}

function list {
  echo "2. list masteraychen, richard.chen, raymond.chen"
  echo ""
  cleos transfer masteraychen $con "100.0000 MGP" "list:2000"
  cleos transfer richard.chen $con "100.0000 MGP" "list:3000"
  cleos transfer raymond.chen $con "100.0000 MGP" "list:5000"
}

function vote {
  echo "3. cast votes..."
  echo ""
  cleos transfer eosio $con "300.0000 MGP" "vote:raymond.chen"
  cleos transfer masteraychen $con "60.0000 MGP" "vote:richard.chen"
}

function r0 {
  echo "Conduct round-0..."
  echo "1) reward w 200 MGP"
  cleos transfer eosio $con "200.0000 MGP"
  echo "2) lists"
  cleos transfer masteraychen $con "10.0000 MGP" "list:2000"
  cleos transfer richard.chen $con "20.0000 MGP" "list:2000"
  cleos transfer raymond.chen $con "100.0000 MGP" "list:4000"
  sleep 3
  echo "3) votes"
  cleos transfer eosio $con "10.0000 MGP" "vote:masteraychen"
  cleos transfer masteraychen $con "30.0000 MGP" "vote:richard.chen"
  cleos transfer masteraychen $con "100.0000 MGP" "vote:raymond.chen"
}

function r1 {
  echo "Conduct round-1..."
  echo "1) reward w 300 MGP"
  cleos transfer eosio $con "300.0000 MGP"
  echo "2) lists - none"
  echo "3) votes"
  cleos transfer eosio $con "10.0000 MGP" "vote:masteraychen"
  cleos transfer masteraychen $con "150.0000 MGP" "vote:richard.chen"
  cleos transfer masteraychen $con "100.0000 MGP" "vote:raymond.chen"
}

function r2 {
  echo "Conduct round-2..."
  echo "1) reward w 200 MGP"
  cleos transfer eosio $con "200.0000 MGP"
  echo "2) lists - none"
  echo "3) votes"
  cleos transfer masteraychen $con "100.0000 MGP" "vote:richard.chen"
  cleos transfer masteraychen $con "100.0000 MGP" "vote:raymond.chen"
  cleos push action masteraychen $con unvote '["raymond.chen", 5]' -p @masteraychen
}

function exec {
  echo "4. execute election..."
  echo ""
  cleos push action $con execute '[]' -p masteraychen
}

function tbl() {
  echo "show table rows..."
  echo ""

  cleos get table $con $con $*
}

. ./env
cmd=$1
#[ -z "$level" ] && level=0

[ $cmd == "setcode" ] && setcode
[ $cmd == "init"   ] && init
[ $cmd == "reward" ] && reward
[ $cmd == "list"   ] && list
[ $cmd == "vote"   ] && vote
[ $cmd == "exec"   ] && exec
[ $cmd == "tbl"    ] && tbl $2 $3
[ $cmd == "global" ] && tbl global $2
[ $cmd == "candidates" ] && tbl candidates $2
[ $cmd == "electrounds" ] && tbl electrounds $2
[ $cmd == "votes" ] && tbl votes $2
[ $cmd == "voters" ] && tbl voters $2
[ $cmd == "rewards" ] && tbl rewards $2
[ $cmd == "r0" ] && r0
[ $cmd == "r1" ] && r1
[ $cmd == "r2" ] && r2
echo ""
###############   END ###################

