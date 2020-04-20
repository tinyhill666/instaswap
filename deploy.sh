URL=http://rpc.yas.plus
ACCOUNT=plusexchange
FILENAME=tokenuniswap
cleos -v -u $URL set abi $ACCOUNT $FILENAME.abi
cleos -v -u $URL set code $ACCOUNT $FILENAME.wasm
