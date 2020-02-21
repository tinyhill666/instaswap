URL=https://rpc.yas.plus
ACCOUNT=plusexchange
FILENAME=tokenuniswap
cleos -u $URL set abi $ACCOUNT $FILENAME.abi
cleos -u $URL set code $ACCOUNT $FILENAME.wasm
