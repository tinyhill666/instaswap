#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>

#define EOS_SYMBOL symbol("EOS", 4)
#define UBI_SYMBOL symbol("UBI", 4)
#define DAPP_TOKEN_CONTRACT "dappubitoken"_n
#define SYSTEM_TOKEN_CONTRACT "eosio.token"_n
#define STORE_CONTRACT "dappubistore"_n
#define FEE_RATE 0.003

using namespace eosio;

CONTRACT tokenuniswap : public contract
{
public:
  using contract::contract;
  tokenuniswap(eosio::name receiver, eosio::name code, datastream<const char *> ds) : contract(receiver, code, ds) {}

  ACTION hi(name user);
  void receive_eos(name from, name to, asset quantity, std::string memo);
  void receive_ubi(name from, name to, asset quantity, std::string memo);

private:
  TABLE tableStruct
  {
    name key;
    std::string name;
  };
  typedef eosio::multi_index<"table"_n, tableStruct> table;
  TABLE liquidity
  {
    name contract;
    asset quantity;
    uint64_t primary_key() const { return contract.value; }
  };
  typedef eosio::multi_index<"liquidity"_n, liquidity> liquidity_index;
};

// EOSIO_DISPATCH(tokenuniswap, (hi))
