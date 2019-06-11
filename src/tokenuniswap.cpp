#include <tokenuniswap.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/action.hpp>
#include <eosio.token/eosio.token.hpp>

ACTION tokenuniswap::hi(name user)
{
  require_auth(user);
  print("Hello, ", name{user});
}

void tokenuniswap::receive_eos(name from, name to, asset quantity, std::string memo)
{
  if (to == get_self() && quantity.symbol == BASE_SYMBOL)
  {
    tokenuniswap::receive_common(from, DIRECTION_BUY, (memo == "add"), DAPP_TOKEN_CONTRACT, UBI_SYMBOL, quantity);
  }
}

void tokenuniswap::receive_ubi(name from, name to, asset quantity, std::string memo)
{
  if (to == get_self() && quantity.symbol == UBI_SYMBOL)
  {
    tokenuniswap::receive_common(from, DIRECTION_SELL, (memo == "add"), DAPP_TOKEN_CONTRACT, UBI_SYMBOL, quantity);
  }
}

void tokenuniswap::receive_common(name user, uint8_t direction, bool isAdd, name token_contract, symbol token_symbol, asset in_quantity)
{
  // get base balance
  double base_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, STORE_CONTRACT, BASE_SYMBOL.code()).amount;

  // get token balance
  double token_balance = eosio::token::get_balance(token_contract, STORE_CONTRACT, token_symbol.code()).amount;

  // add liquidity logic
  if (isAdd)
  {
    liquidity_index liquidityRecords(get_self(), user.value);
    auto existing = direction == DIRECTION_BUY ? liquidityRecords.find(token_contract.value) : liquidityRecords.find(SYSTEM_TOKEN_CONTRACT.value);
    // not exsit
    if (existing == liquidityRecords.end())
    {
      liquidityRecords.emplace(_self, [&](auto &record) {
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
            std::make_tuple(get_self(), STORE_CONTRACT, asset(token_add_amount, token_symbol), std::string("Add liquidity to pool")))
            .send();
      }
      if (base_add_amount > 0)
      {
        action(
            permission_level{get_self(), "active"_n},
            SYSTEM_TOKEN_CONTRACT,
            "transfer"_n,
            std::make_tuple(get_self(), STORE_CONTRACT, asset(base_add_amount, BASE_SYMBOL), std::string("Add liquidity to pool")))
            .send();
      }
      if (token_back_amount > 0)
      {
        action(
            permission_level{get_self(), "active"_n},
            token_contract,
            "transfer"_n,
            std::make_tuple(get_self(), user, asset(token_back_amount, token_symbol), std::string("Change for Adding liquidity to pool")))
            .send();
      }
      if (base_back_amount > 0)
      {
        action(
            permission_level{get_self(), "active"_n},
            SYSTEM_TOKEN_CONTRACT,
            "transfer"_n,
            std::make_tuple(get_self(), user, asset(base_back_amount, BASE_SYMBOL), std::string("Change for Adding liquidity to pool")))
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
        permission_level{STORE_CONTRACT, "active"_n},
        out_contract,
        "transfer"_n,
        std::make_tuple(STORE_CONTRACT, user, out_quantity, std::string("Exchange with Uniswap")))
        .send();

    // transfer fund to store
    action(
        permission_level{get_self(), "active"_n},
        in_contract,
        "transfer"_n,
        std::make_tuple(get_self(), STORE_CONTRACT, in_quantity, std::string("Transfer fund to store")))
        .send();

    return;
  }
}

extern "C"
{
  void apply(uint64_t receiver, uint64_t code, uint64_t action)
  {
    if (code == SYSTEM_TOKEN_CONTRACT.value && action == "transfer"_n.value)
    {
      eosio::execute_action(
          name(receiver), name(code), &tokenuniswap::receive_eos);
    }
    if (code == DAPP_TOKEN_CONTRACT.value && action == "transfer"_n.value)
    {
      eosio::execute_action(
          name(receiver), name(code), &tokenuniswap::receive_ubi);
    }

    // if (code == receiver) {
    //   switch (action) {
    //     EOSIO_DISPATCH_HELPER(bankofstaked,
    //       (clearhistory)
    //       (empty)
    //       (test)
    //       (rotate)
    //       (check)
    //       (forcexpire)
    //       (expireorder)
    //       (addwhitelist)
    //       (delwhitelist)
    //       (addcreditor)
    //       (addsafeacnt)
    //       (delsafeacnt)
    //       (delcreditor)
    //       (addblacklist)
    //       (delblacklist)
    //       (setplan)
    //       (activate)
    //       (activateplan)
    //       (setrecipient)
    //       (delrecipient)
    //       (customorder)
    //     )
    //   }
    // }
  }
}