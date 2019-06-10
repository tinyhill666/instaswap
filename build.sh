eosio-cpp --contract tokenuniswap \
    -abigen src/tokenuniswap.cpp \
    -o tokenuniswap.wasm \
    -I ~/dev/eos/eosio.contracts/eosio.token/include \
    -I ./include
