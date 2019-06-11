#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>

#define BASE_SYMBOL symbol("EOS", 4)
#define UBI_SYMBOL symbol("UBI", 4)
// #define SELF_CONTRACT "dappubilogic"_n
#define DAPP_TOKEN_CONTRACT "dappubitoken"_n
#define SYSTEM_TOKEN_CONTRACT "eosio.token"_n
#define STORE_CONTRACT "dappubistore"_n
#define FEE_RATE 0.003
#define DIRECTION_BUY 0
#define DIRECTION_SELL 1

using namespace eosio;

CONTRACT tokenuniswap : public contract
{
public:
  using contract::contract;
  tokenuniswap(eosio::name receiver, eosio::name code, datastream<const char *> ds) : contract(receiver, code, ds) {}

  ACTION create(name token_contract, asset quantity, name store_account);
  void receive_base(name from, name to, asset quantity, std::string memo);
  void receive_token(name from, name to, asset quantity, std::string memo);
  void receive_common(name user, uint8_t direction, bool isAdd, name token_contract, symbol token_symbol, asset in_quantity);

private:
  TABLE liquidity
  {
    name contract;
    asset quantity;
    uint64_t primary_key() const { return contract.value; }
  };
  typedef eosio::multi_index<"liquidity"_n, liquidity> liquidity_index;

  TABLE market
  {
    name store;
    name contract;
    asset target;
    uint64_t total_share;
    uint64_t primary_key() const { return store.value; }
    uint128_t by_token() const
    {
      return (uint128_t(contract.value) << 64) + target.symbol.raw();
    }
    //uint64_t primary_key() const { return target.symbol.raw(); }
  };
  typedef eosio::multi_index<"markets"_n, market> market_index;
  typedef eosio::multi_index<"markets"_n, market, indexed_by<"bytoken"_n, const_mem_fun<market, uint128_t, &market::by_token>>> market_index_by_token;

  TABLE share
  {
    uint64_t market_id;
    uint64_t my_share;
    uint64_t primary_key() const { return market_id; }
  };
  typedef eosio::multi_index<"shares"_n, share> share_index;
};