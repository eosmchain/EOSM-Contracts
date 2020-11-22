function deploy {
    cd /opt/src/MGP-Contracts && cleos set contract $con ./build/contracts/mgp.bpvoting/
    cleos set account permission $con active '{"threshold": 1,"keys": [{"key": "'$pubkey'","weight": 1}],"accounts": [{"permission":{"actor":"'$con'","permission":"eosio.code"},"weight":1}]}' owner -p $con

}

function init {
  echo "0. conf & init"
  echo ""
  cleos push action $con config '[20,30,50,21,180,600,1,"100.0000 MGP","200.0000 MGP","10.0000 MGP"]' -p $con
  cleos push action $con init '[]' -p $con
  cleos push action $con setelect '[0,0]' -p $con 
}

function reward {
  echo "1. send rewards"
  echo ""
  cleos transfer eosio $con "1000.0000 MGP" 
  cleos transfer eosio $con "500.0000 MGP" 
  cleos transfer eosio $con "50.0000 MGP" 
}

function list {
  echo "2. list masteraychen, richard.chen, raymond.chen"
  echo ""
  cleos transfer masteraychen $con "1000.0000 MGP" "list:2000"
  cleos transfer richard.chen $con "200.0000 MGP" "list:3000"
  cleos transfer raymond.chen $con "3000.0000 MGP" "list:5000"
}

function vote {
  echo "3. cast votes..."
  echo ""
  cleos transfer eosio $con "100.0000 MGP" "vote:masteraychen"
  cleos transfer eosio $con "300.0000 MGP" "vote:raymond.chen"
  cleos transfer masteraychen $con "30.0000 MGP" "vote:raymond.chen"
  cleos transfer masteraychen $con "60.0000 MGP" "vote:richard.chen"
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

[ $cmd == "deploy" ] && deploy
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

echo ""
###############   END ###################

