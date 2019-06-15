#include <tokenuniswap.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/action.hpp>
#include <eosio.token/eosio.token.hpp>

ACTION tokenuniswap::create(name token_contract, asset quantity, name store_account)
{
  require_auth(get_self());

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
    record.store = store_account;
    record.contract = token_contract;
    record.target = quantity;
    record.total_share = 0;
  });

  // check auth of store account
  action(
      permission_level{get_self(), "active"_n},
      SYSTEM_TOKEN_CONTRACT,
      "transfer"_n,
      make_tuple(get_self(), store_account, asset(1, BASE_SYMBOL), string("test auth")))
      .send();
  action(
      permission_level{store_account, "active"_n},
      SYSTEM_TOKEN_CONTRACT,
      "transfer"_n,
      make_tuple(store_account, token_contract, asset(1, BASE_SYMBOL), string("test auth")))
      .send();
}

void tokenuniswap::receive_pretreatment(name from, name to, asset quantity, string memo)
{
  // important check
  if (to != get_self())
  //
  {
    return;
  }
  else
  {
    // parse memo function_name | target_store_account
    vector<string> splits;
    split_string(memo, splits, "|");
    string function_name = splits[0];
    string target_store_account_string = splits[1];

    // get target market info
    tokenuniswap::market_index _market(get_self(), get_self().value);
    auto target_market_item = _market.get(name(target_store_account_string).value);

    //important check
    // receive system token
    if (get_code() == SYSTEM_TOKEN_CONTRACT && quantity.symbol == BASE_SYMBOL)
    //
    {
      tokenuniswap::receive_dispatcher(from, DIRECTION_BUY, function_name, target_market_item.store, target_market_item.contract, target_market_item.target.symbol, quantity);
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
          if (target_market_item.store == item.store)
          {
            tokenuniswap::receive_dispatcher(from, DIRECTION_SELL, function_name, item.store, item.contract, item.target.symbol, quantity);
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

void tokenuniswap::receive_dispatcher(name user, uint8_t direction, string function_name, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{
  // add liquidity logic
  if (function_name == "add")
  {
    tokenuniswap::add_liquidity(user, direction, store_account, token_contract, token_symbol, in_quantity);
    return;
  }
  else if (function_name == "exchange")
  {
    tokenuniswap::exchange(user, direction, store_account, token_contract, token_symbol, in_quantity);
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
  auto existing = direction == DIRECTION_BUY ? _liquidity.find(token_contract.value) : _liquidity.find(SYSTEM_TOKEN_CONTRACT.value);
  // not exsit . add liquidity step1
  if (existing == _liquidity.end())
  {
    _liquidity.emplace(get_self(), [&](auto &record) {
      record.contract = direction == DIRECTION_BUY ? SYSTEM_TOKEN_CONTRACT : token_contract;
      record.quantity = in_quantity;
    });
  }
  //add liquidity step2
  else
  {
    // erase step1 info
    _liquidity.erase(existing);

    // get base balance
    auto base_balance_amount = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, store_account, BASE_SYMBOL.code()).amount;
    eosio::check(base_balance_amount != 0, "base balance can not be 0");
    double base_balance = base_balance_amount;

    // get token balance
    double token_balance = eosio::token::get_balance(token_contract, store_account, token_symbol.code()).amount;

    auto base_quantity = direction == DIRECTION_BUY ? in_quantity : existing->quantity;
    auto token_quantity = direction == DIRECTION_BUY ? existing->quantity : in_quantity;
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
        record.market_id = store_account;
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

    action(
        permission_level{get_self(), "active"_n},
        token_contract,
        "transfer"_n,
        make_tuple(get_self(), store_account, asset(token_add_amount, token_symbol), string("Add liquidity to pool")))
        .send();

    action(
        permission_level{get_self(), "active"_n},
        SYSTEM_TOKEN_CONTRACT,
        "transfer"_n,
        make_tuple(get_self(), store_account, asset(base_add_amount, BASE_SYMBOL), string("Add liquidity to pool")))
        .send();

    if (token_back_amount > 0)
    {
      action(
          permission_level{get_self(), "active"_n},
          token_contract,
          "transfer"_n,
          make_tuple(get_self(), user, asset(token_back_amount, token_symbol), string("Change for Adding liquidity to pool")))
          .send();
    }
    if (base_back_amount > 0)
    {
      action(
          permission_level{get_self(), "active"_n},
          SYSTEM_TOKEN_CONTRACT,
          "transfer"_n,
          make_tuple(get_self(), user, asset(base_back_amount, BASE_SYMBOL), string("Change for Adding liquidity to pool")))
          .send();
    }
  }
  return;
}

void tokenuniswap::exchange(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{

  // get base balance
  auto base_balance_amount = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, store_account, BASE_SYMBOL.code()).amount;
  eosio::check(base_balance_amount != 0, "base balance can not be 0");
  double base_balance = base_balance_amount;

  // get token balance
  double token_balance = eosio::token::get_balance(token_contract, store_account, token_symbol.code()).amount;

  //deduct fee
  double received = in_quantity.amount;
  received = received * (1 - FEE_RATE);

  double product = token_balance * base_balance;
  asset out_quantity;
  name out_contract, in_contract;
  if (direction == DIRECTION_BUY)
  {
    auto out_amount = token_balance - (product / (received + base_balance));
    out_quantity = asset(out_amount, token_symbol);
    out_contract = token_contract;
    in_contract = SYSTEM_TOKEN_CONTRACT;
  }
  else
  {
    auto out_amount = base_balance - (product / (received + token_balance));
    out_quantity = asset(out_amount, BASE_SYMBOL);
    out_contract = SYSTEM_TOKEN_CONTRACT;
    in_contract = token_contract;
  }

  // transfer to user
  action(
      permission_level{store_account, "active"_n},
      out_contract,
      "transfer"_n,
      make_tuple(store_account, user, out_quantity, string("Exchange with Uniswap")))
      .send();

  // transfer fund to store
  action(
      permission_level{get_self(), "active"_n},
      in_contract,
      "transfer"_n,
      make_tuple(get_self(), store_account, in_quantity, string("Transfer fund to store")))
      .send();

  return;
}

void tokenuniswap::init_liquidity(name user, uint8_t direction, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{

  liquidity_index _liquidity(get_self(), user.value);
  auto existing = direction == DIRECTION_BUY ? _liquidity.find(token_contract.value) : _liquidity.find(SYSTEM_TOKEN_CONTRACT.value);
  // not exsit . add liquidity step1
  if (existing == _liquidity.end())
  {
    _liquidity.emplace(get_self(), [&](auto &record) {
      record.contract = direction == DIRECTION_BUY ? SYSTEM_TOKEN_CONTRACT : token_contract;
      record.quantity = in_quantity;
    });
  }
  //add liquidity step2
  else
  {
    // erase step1 info
    _liquidity.erase(existing);

    auto base_quantity = direction == DIRECTION_BUY ? in_quantity : existing->quantity;
    auto token_quantity = direction == DIRECTION_BUY ? existing->quantity : in_quantity;
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
      record.market_id = store_account;
      record.my_share = add_share;
    });

    _market.modify(market, get_self(), [&](auto &record) {
      record.total_share = add_share;
    });

    // transfer fund to store
    action(
        permission_level{get_self(), "active"_n},
        token_contract,
        "transfer"_n,
        make_tuple(get_self(), store_account, asset(token_amount, token_symbol), string("Init liquidity pool")))
        .send();

    action(
        permission_level{get_self(), "active"_n},
        SYSTEM_TOKEN_CONTRACT,
        "transfer"_n,
        make_tuple(get_self(), store_account, asset(base_amount, BASE_SYMBOL), string("Init liquidity pool")))
        .send();

    // check pool balance
    // double base_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, store_account, BASE_SYMBOL.code()).amount;
    // eosio::check(base_balance == base_amount, (string("Liquidity pool should be empty before init") + to_string(base_balance)).c_str());
    // double token_balance = eosio::token::get_balance(token_contract, store_account, token_symbol.code()).amount;
    // eosio::check(token_balance == token_amount, "Liquidity pool should be empty before init");
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
        EOSIO_DISPATCH_HELPER(tokenuniswap,
                              (create))
      }
    }
  }
}