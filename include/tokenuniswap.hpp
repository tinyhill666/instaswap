#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>

#define BASE_SYMBOL symbol("EOS", 4)
#define SYSTEM_TOKEN_CONTRACT "eosio.token"_n
#define FEE_RATE 0.003
#define DIRECTION_BUY 0
#define DIRECTION_SELL 1

using namespace eosio;
using namespace std;

CONTRACT tokenuniswap : public contract
{
  //table
  TABLE liquidity
  {
    name market_name;
    asset quantity;
    uint64_t primary_key() const { return market_name.value; }
  };
  typedef eosio::multi_index<"liquidity"_n, liquidity> liquidity_index;

  // makert id is store account
  TABLE market
  {
    name market_name;
    name contract;
    asset target;
    uint64_t total_share;
    uint64_t primary_key() const { return market_name.value; }
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
    name market_name;
    uint64_t my_share;
    uint64_t primary_key() const { return market_name.value; }
  };
  typedef eosio::multi_index<"shares"_n, share> share_index;

  TABLE global
  {
    bool maintain;
  };
  typedef eosio::singleton<"global"_n, global> global_state_singleton;

public:
  using contract::contract;
  tokenuniswap(eosio::name receiver, eosio::name code, datastream<const char *> ds)
      : contract(receiver, code, ds),
        _global(get_self(), get_self().value)
  {
  }

  ACTION create(name token_contract, asset quantity, name store_account);
  ACTION reset(name scope);
  ACTION init();
  ACTION maintain(bool is_maintain);
  void receive_pretreatment(name from, name to, asset quantity, string memo);
  void receive_dispatcher(name user, uint8_t direction, string function_name, name store_account, name token_contract, symbol token_symbol, asset in_quantity);
  void init_liquidity(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity);
  void add_liquidity(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity);
  void exchange(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity);

private:
  global_state_singleton _global;
  void create_account(name store_account)
  {
    // create account

    using eosio::permission_level;
    using eosio::public_key;

    struct permission_level_weight
    {
      permission_level permission;
      uint16_t weight;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      // EOSLIB_SERIALIZE(permission_level_weight, (permission)(weight))
    };

    struct key_weight
    {
      eosio::public_key key;
      uint16_t weight;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      // EOSLIB_SERIALIZE(key_weight, (key)(weight))
    };

    struct wait_weight
    {
      uint32_t wait_sec;
      uint16_t weight;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      // EOSLIB_SERIALIZE(wait_weight, (wait_sec)(weight))
    };

    struct authority
    {
      uint32_t threshold = 0;
      std::vector<key_weight> keys;
      std::vector<permission_level_weight> accounts;
      std::vector<wait_weight> waits;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      // EOSLIB_SERIALIZE(authority, (threshold)(keys)(accounts)(waits))
    };

    // owner key
    permission_level owner_permission_level = permission_level(get_self(), "active"_n);
    permission_level_weight owner_permission_level_weight = {.permission = owner_permission_level, .weight = 1};
    vector<permission_level_weight> owner_accounts{owner_permission_level_weight};
    authority owner_authority = {.threshold = 1, .keys = {}, .accounts = owner_accounts, .waits = {}};

    // active key
    permission_level active_permission_level = permission_level(get_self(), "eosio.code"_n);
    permission_level_weight active_permission_level_weight = {.permission = active_permission_level, .weight = 1};
    vector<permission_level_weight> active_accounts{active_permission_level_weight};
    authority active_authority = {.threshold = 1, .keys = {}, .accounts = active_accounts, .waits = {}};

    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "newaccount"_n,
        make_tuple(get_self(), store_account, owner_authority, active_authority))
        .send();

    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "buyrambytes"_n,
        make_tuple(get_self(), store_account, 5000))
        .send();
  }
  void inline_transfer(name token_contract, name from, name to, asset qunatity, string memo)
  {
    action(
        permission_level{from, "active"_n},
        token_contract,
        "transfer"_n,
        make_tuple(from, to, qunatity, memo))
        .send();
  }

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
};