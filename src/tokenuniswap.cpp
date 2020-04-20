/**
 * TODO
 * support token not 4 precesion
 * token to token swap
 * add price guarantee
 */
#include <tokenuniswap.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/action.hpp>
#include <eosio.token.hpp>
#include <eosiolib/public_key.hpp>
#include <string>

ACTION tokenuniswap::init()
{
  require_auth(get_self());

  tokenuniswap::global _global_state = {.maintain = 0, .lock = 0};
  _global.set(_global_state, get_self());

}
ACTION tokenuniswap::reset(name scope)
{
  require_auth(get_self());

  liquidity_index _liquidity(get_self(), scope.value);
  auto liquidity_itr = _liquidity.begin();
  while (liquidity_itr != _liquidity.end())
  {
    _liquidity.erase(liquidity_itr);
    liquidity_itr = _liquidity.begin();
  }

  market_index _market(get_self(), scope.value);
  auto market_itr = _market.begin();
  while (market_itr != _market.end())
  {
    _market.erase(market_itr);
    market_itr = _market.begin();
  }

  share_index _share(get_self(), scope.value);
  auto share_itr = _share.begin();
  while (share_itr != _share.end())
  {
    _share.erase(share_itr);
    share_itr = _share.begin();
  }
}

ACTION tokenuniswap::maintain(bool is_maintain)
{
  require_auth(get_self());

  eosio::check(_global.exists(), "please init first");
  auto _global_state = _global.get();
  _global_state.maintain = is_maintain;
  _global.set(_global_state, get_self());
}

ACTION tokenuniswap::create(name token_contract, asset quantity, name store_account)
{
  require_auth(get_self());
  // require_auth(creator);

  // only support token of 4 digit precesion
  eosio::check(quantity.symbol.precision() == 4, "only support token of 4 digit precesion");

  quantity.amount = 0;

  // check if exchange pair already exist
  tokenuniswap::market_index_by_token marketlist(get_self(), get_self().value);
  for (auto &item : marketlist)
  {
    if (token_contract == item.contract && quantity.symbol == item.target.symbol)
    {
      eosio::check(false, "market already exsit!");
      return;
    }
  }

  marketlist.emplace(get_self(), [&](auto &record) {
    record.market_name = store_account;
    record.contract = token_contract;
    record.target = quantity;
    record.total_share = 0;
  });

  tokenuniswap::create_account(store_account);
}

ACTION tokenuniswap::withdraw(name user, name market_name, uint64_t withdraw_share)
{
  require_auth(user);

  // get share info
  share_index _share(get_self(), user.value);
  market_index _market(get_self(), get_self().value);

  auto market = _market.find(market_name.value);
  eosio::check(market != _market.end(), "can not get market info");
  auto share = _share.find(market_name.value);
  eosio::check(share != _share.end(), "can not get share info");

  // pool can not empty
  auto total_share = market->total_share;
  eosio::check(total_share > withdraw_share, "pool can not empty");

  auto my_share = share->my_share;
  eosio::check(my_share >= withdraw_share, "share not enough");

  // get base balance
  double base_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, market_name, BASE_SYMBOL.code()).amount;
  // get token balance
  double token_balance = eosio::token::get_balance(market->contract, market_name, market->target.symbol.code()).amount;

  auto withdraw_base_amount = base_balance / total_share * withdraw_share;
  auto withdraw_token_amount = token_balance / total_share * withdraw_share;

  if (my_share == withdraw_share)
  {
    _share.erase(share);
  }
  else
  {
    //use user ram
    _share.modify(share, user, [&](auto &record) {
      record.my_share -= withdraw_share;
    });
  }

  _market.modify(market, get_self(), [&](auto &record) {
    record.total_share -= withdraw_share;
  });

  // transfer fund to user
  tokenuniswap::inline_transfer(market->contract, market_name, user, asset(withdraw_token_amount, market->target.symbol), string("Withdraw liquidity from pool"));
  tokenuniswap::inline_transfer(SYSTEM_TOKEN_CONTRACT, market_name, user, asset(withdraw_base_amount, BASE_SYMBOL), string("Withdraw liquidity from pool"));
}

void tokenuniswap::receive_pretreatment(name from, name to, asset quantity, string memo)
{
  // stop receiving when in maintain
  eosio::check(_global.exists(), "please init first");
  auto _global_state = _global.get();
  eosio::check(!_global_state.maintain, "contract in maintain");

  if (from == get_self())
  {
    // unlock
    _global_state.lock = false;
    _global.set(_global_state, get_self());
    return;
  }

  // important check
  if (to != get_self())
  //
  {
    return;
  }
  else
  {
    // check reenter
    eosio::check(!_global_state.lock, "can not re enter");

    _global_state.lock = true;
    _global.set(_global_state, get_self());

    // parse memo function_name | target_store_account
    vector<string> splits;
    split_string(memo, splits, "|");
    string function_name = splits[0];
    if (function_name == "nothing")
    {
      // just add balance to contract
      return;
    }
    string target_store_account_string = splits[1];

    // get target market info
    tokenuniswap::market_index _market(get_self(), get_self().value);
    auto target_market_item = _market.get(name(target_store_account_string).value);

    //important check
    // receive system token
    if (get_code() == SYSTEM_TOKEN_CONTRACT && quantity.symbol == BASE_SYMBOL)
    //
    {
      tokenuniswap::receive_dispatcher(splits, from, DIRECTION_BUY, function_name, target_market_item.market_name, target_market_item.contract, target_market_item.target.symbol, quantity);
    }
    // receive dapp token
    else
    {
      tokenuniswap::market_index_by_token marketlist(get_self(), get_self().value);
      for (auto &item : marketlist)
      {
        // important check
        if (get_code() == item.contract && quantity.symbol == item.target.symbol)
        {
          // exchange token with base token
          if (target_market_item.market_name == item.market_name)
          {
            tokenuniswap::receive_dispatcher(splits, from, DIRECTION_SELL, function_name, item.market_name, item.contract, item.target.symbol, quantity);
          }
          // exhcange token with another token
          else
          {
            // todo
          }
        }
      }
    }
  }
}

void tokenuniswap::receive_dispatcher(vector<string> parameters, name user, uint8_t direction, string function_name, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{
  // add liquidity logic
  if (function_name == "add")
  {
    tokenuniswap::add_liquidity(user, direction, store_account, token_contract, token_symbol, in_quantity);
    return;
  }
  else if (function_name == "exchange")
  {
    tokenuniswap::exchange(parameters, user, direction, store_account, token_contract, token_symbol, in_quantity);
    return;
  }
  else if (function_name == "init")
  {
    tokenuniswap::init_liquidity(user, direction, store_account, token_contract, token_symbol, in_quantity);
    return;
  }
}

void tokenuniswap::add_liquidity(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{

  liquidity_index _liquidity(get_self(), user.value);
  auto existing = _liquidity.find(store_account.value);
  // not exsit . add liquidity step1 , add token too contract
  if (existing == _liquidity.end())
  {
    eosio::check(direction == DIRECTION_SELL, "send token first");

    _liquidity.emplace(get_self(), [&](auto &record) {
      record.market_name = store_account;
      record.quantity = in_quantity;
    });
  }
  //add liquidity step2
  else
  {
    eosio::check(direction == DIRECTION_BUY, "send EOS in step 2");

    // erase step1 info
    _liquidity.erase(existing);

    // get base balance
    auto base_balance_amount = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, store_account, BASE_SYMBOL.code()).amount;
    eosio::check(base_balance_amount != 0, "base balance can not be 0");
    double base_balance = base_balance_amount;

    // get token balance
    double token_balance = eosio::token::get_balance(token_contract, store_account, token_symbol.code()).amount;

    auto base_quantity = in_quantity;
    auto token_quantity = existing->quantity;
    auto base_amount = base_quantity.amount;
    auto token_amount = token_quantity.amount;
    double price = double(token_balance) / base_balance;

    uint64_t base_add_amount, token_add_amount;
    // base amount enough
    if (double(token_amount) / base_amount <= price)
    {
      base_add_amount = double(token_amount) / price;
      token_add_amount = token_amount;
    }
    else
    {
      base_add_amount = base_amount;
      token_add_amount = double(base_amount) * price;
    }
    uint64_t base_back_amount = base_amount - base_add_amount;
    uint64_t token_back_amount = token_amount - token_add_amount;

    // change share info
    share_index _share(get_self(), user.value);
    market_index _market(get_self(), get_self().value);
    auto market = _market.find(store_account.value);
    eosio::check(market != _market.end(), "can not get market info");
    auto share = _share.find(store_account.value);
    auto total_share = market->total_share;
    if (total_share == 0)
    {
      total_share = base_add_amount;
    }
    auto add_share = total_share * base_add_amount / base_balance;

    // how to use user ram
    if (share == _share.end())
    {
      _share.emplace(get_self(), [&](auto &record) {
        record.market_name = store_account;
        record.my_share = add_share;
      });
    }
    else
    {
      _share.modify(share, get_self(), [&](auto &record) {
        record.my_share += add_share;
      });
    }

    _market.modify(market, get_self(), [&](auto &record) {
      record.total_share = total_share + add_share;
    });

    // transfer fund to store
    tokenuniswap::inline_transfer(token_contract, get_self(), store_account, asset(token_add_amount, token_symbol), string("Add liquidity to pool"));
    tokenuniswap::inline_transfer(SYSTEM_TOKEN_CONTRACT, get_self(), store_account, asset(base_add_amount, BASE_SYMBOL), string("Add liquidity to pool"));

    // return unmatch amount
    if (token_back_amount > 0)
    {
      tokenuniswap::inline_transfer(token_contract, get_self(), user, asset(token_back_amount, token_symbol), string("Change for Adding liquidity to pool"));
    }
    if (base_back_amount > 0)
    {
      tokenuniswap::inline_transfer(SYSTEM_TOKEN_CONTRACT, get_self(), user, asset(base_back_amount, BASE_SYMBOL), string("Change for Adding liquidity to pool"));
    }
  }
  return;
}

void tokenuniswap::exchange(vector<string> parameters, name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{
  // get base balance
  double base_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, store_account, BASE_SYMBOL.code()).amount;
  eosio::check(base_balance != 0, "base balance can not be 0");

  // get token balance
  double token_balance = eosio::token::get_balance(token_contract, store_account, token_symbol.code()).amount;

  //deduct fee
  double received = in_quantity.amount;
  received = received * (1 - FEE_RATE);

  double product = token_balance * base_balance;
  double out_amount;
  asset out_quantity;
  name out_contract, in_contract;

  if (direction == DIRECTION_BUY)
  {
    eosio::check((received / base_balance) < MAX_CHANGE_RATE, "exchange amount too big, price change too much");
    out_amount = token_balance - (product / (received + base_balance));
    out_quantity = asset(out_amount, token_symbol);
    out_contract = token_contract;
    in_contract = SYSTEM_TOKEN_CONTRACT;
  }
  else
  {
    eosio::check((received / token_balance) < MAX_CHANGE_RATE, "exchange amount too big, price change too much");
    out_amount = base_balance - (product / (received + token_balance));
    out_quantity = asset(out_amount, BASE_SYMBOL);
    out_contract = SYSTEM_TOKEN_CONTRACT;
    in_contract = token_contract;
  }

  // get guarantee price
  if (parameters.size() > 2)
  {
    double price = received / out_amount;
    double guarantee_price = tokenuniswap::str_to_double(parameters[2]);
    if (direction == DIRECTION_BUY)
    {
      eosio::check(price < guarantee_price, "price not match");
    }
    else
    {
      eosio::check(price > guarantee_price, "price not match");
    }
  }

  // transfer to user
  tokenuniswap::inline_transfer(out_contract, store_account, user, out_quantity, string("Exchange with Uniswap"));

  // transfer fund to store
  tokenuniswap::inline_transfer(in_contract, get_self(), store_account, in_quantity, string("Transfer fund to store"));

  return;
}

void tokenuniswap::init_liquidity(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{
  liquidity_index _liquidity(get_self(), user.value);
  auto existing = _liquidity.find(store_account.value);
  // not exsit . add liquidity step1 , add token too contract
  if (existing == _liquidity.end())
  {
    eosio::check(direction == DIRECTION_SELL, "send token first");

    _liquidity.emplace(get_self(), [&](auto &record) {
      record.market_name = store_account;
      record.quantity = in_quantity;
    });
  }
  //add liquidity step2
  else
  {
    eosio::check(direction == DIRECTION_BUY, "send EOS in step 2");
    // erase step1 info
    _liquidity.erase(existing);

    auto base_quantity = in_quantity;
    auto token_quantity = existing->quantity;
    auto base_amount = base_quantity.amount;
    auto token_amount = token_quantity.amount;

    // init share info
    share_index _share(get_self(), user.value);
    market_index _market(get_self(), get_self().value);
    auto market = _market.find(store_account.value);
    eosio::check(market != _market.end(), "can not get market info");
    eosio::check(market->total_share == 0, "market total share should be 0 when init");
    auto share = _share.find(store_account.value);
    eosio::check(share == _share.end(), "user share info should not exist");

    auto total_share = base_amount;
    auto add_share = base_amount;

    _share.emplace(get_self(), [&](auto &record) {
      record.market_name = store_account;
      record.my_share = add_share;
    });

    _market.modify(market, get_self(), [&](auto &record) {
      record.total_share = add_share;
    });

    // transfer fund to store
    tokenuniswap::inline_transfer(token_contract, get_self(), store_account, asset(token_amount, token_symbol), string("Init liquidity pool"));
    tokenuniswap::inline_transfer(SYSTEM_TOKEN_CONTRACT, get_self(), store_account, asset(base_amount, BASE_SYMBOL), string("Init liquidity pool"));
  }
  return;
}

extern "C"
{
  void apply(uint64_t receiver, uint64_t code, uint64_t action)
  {
    if (code != receiver && action == "transfer"_n.value)
    {
      eosio::execute_action(
          name(receiver), name(code), &tokenuniswap::receive_pretreatment);
    }

    if (code == receiver)
    {
      switch (action)
      {
        EOSIO_DISPATCH_HELPER(
            tokenuniswap,
            (create)(init)(reset)(maintain)(withdraw))
      }
    }
  }
}