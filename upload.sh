host=m1
#host=jw
scp -r ./build/contracts ${host}:/opt/mgp/wallet
scp ./unittest.sh ${host}:/opt/mgp/wallet/