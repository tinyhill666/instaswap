#include <tokenuniswap.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/action.hpp>
#include <eosio.token/eosio.token.hpp>

ACTION tokenuniswap::create(name token_contract, asset quantity, name store_account)
{
  require_auth(get_self());
  quantity.amount = 0;
  market_index markets(get_self(), get_self().value);
  markets.emplace(get_self(), [&](auto &record) {
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

void tokenuniswap::receive_token(name from, name to, asset quantity, string memo)
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
      tokenuniswap::receive_common(from, DIRECTION_BUY, function_name, target_market_item.store, target_market_item.contract, target_market_item.target.symbol, quantity);
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
            tokenuniswap::receive_common(from, DIRECTION_SELL, function_name, item.store, item.contract, item.target.symbol, quantity);
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

void tokenuniswap::receive_common(name user, uint8_t direction, string function_name, name store_account, name token_contract, symbol token_symbol, asset in_quantity)
{
  // get base balance
  double base_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, store_account, BASE_SYMBOL.code()).amount;

  // get token balance
  double token_balance = eosio::token::get_balance(token_contract, store_account, token_symbol.code()).amount;

  // add liquidity logic
  if (function_name == "add")
  {
    liquidity_index liquidityRecords(get_self(), user.value);
    auto existing = direction == DIRECTION_BUY ? liquidityRecords.find(token_contract.value) : liquidityRecords.find(SYSTEM_TOKEN_CONTRACT.value);
    // not exsit
    if (existing == liquidityRecords.end())
    {
      liquidityRecords.emplace(get_self(), [&](auto &record) {
        record.contract = direction == DIRECTION_BUY ? SYSTEM_TOKEN_CONTRACT : token_contract;
        record.quantity = in_quantity;
      });
    }
    else
    {
      liquidityRecords.erase(existing);

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

      // transfer fund to store
      if (token_add_amount > 0)
      {
        action(
            permission_level{get_self(), "active"_n},
            token_contract,
            "transfer"_n,
            make_tuple(get_self(), store_account, asset(token_add_amount, token_symbol), string("Add liquidity to pool")))
            .send();
      }
      if (base_add_amount > 0)
      {
        action(
            permission_level{get_self(), "active"_n},
            SYSTEM_TOKEN_CONTRACT,
            "transfer"_n,
            make_tuple(get_self(), store_account, asset(base_add_amount, BASE_SYMBOL), string("Add liquidity to pool")))
            .send();
      }
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
  // exchange logic
  else
  {
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
}

extern "C"
{
  void apply(uint64_t receiver, uint64_t code, uint64_t action)
  {
    if (code != receiver && action == "transfer"_n.value)
    {
      eosio::execute_action(
          name(receiver), name(code), &tokenuniswap::receive_token);
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