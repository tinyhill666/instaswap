#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>

#define BASE_SYMBOL symbol("EOS", 4)
#define SYSTEM_TOKEN_CONTRACT "eosio.token"_n
#define FEE_RATE 0.003
#define DIRECTION_BUY 0
#define DIRECTION_SELL 1

using namespace eosio;
using namespace std;

CONTRACT tokenuniswap : public contract
{
public:
  using contract::contract;
  tokenuniswap(eosio::name receiver, eosio::name code, datastream<const char *> ds) : contract(receiver, code, ds) {}

  ACTION create(name token_contract, asset quantity, name store_account);
  void receive_pretreatment(name from, name to, asset quantity, string memo);
  void receive_dispatcher(name user, uint8_t direction, string function_name, name store_account, name token_contract, symbol token_symbol, asset in_quantity);
  void add_liquidity(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity);
  void exchange(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity);

private:
  void split_string(const string &s, vector<string> &v, const string &c)
  {
    string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while (string::npos != pos2)
    {
      v.push_back(s.substr(pos1, pos2 - pos1));

      pos1 = pos2 + c.size();
      pos2 = s.find(c, pos1);
    }
    if (pos1 != s.length())
      v.push_back(s.substr(pos1));
  }

  //table
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