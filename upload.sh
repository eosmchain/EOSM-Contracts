host=m1
# host=jw

rsync -rav -e ssh --include='*.abi' --include='*.wasm' \
    --exclude='unittest.sh' --exclude='**/CMakeFiles' \
    --exclude='*.cmake' --exclude='Makefile' --exclude='*.md' \
    ./build/contracts ${host}:/opt/mgp/wallet

rsync -rav -e ssh ./unittest.sh ${host}:/opt/mgp/wallet/