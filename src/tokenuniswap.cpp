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
  if (to == get_self() && quantity.symbol == EOS_SYMBOL)
  {
    // get EOS balance
    double eos_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, STORE_CONTRACT, EOS_SYMBOL.code()).amount;

    // get UBI balance
    double ubi_balance = eosio::token::get_balance(DAPP_TOKEN_CONTRACT, STORE_CONTRACT, UBI_SYMBOL.code()).amount;

    // add liquidity logic
    if (memo == "add")
    {
      liquidity_index liquidityRecords(get_self(), from.value);
      auto existing = liquidityRecords.find(DAPP_TOKEN_CONTRACT.value);
      // not exsit
      if (existing == liquidityRecords.end())
      {
        liquidityRecords.emplace(_self, [&](auto &record) {
          record.contract = SYSTEM_TOKEN_CONTRACT;
          record.quantity = quantity;
        });
      }
      else
      {
        liquidityRecords.erase(existing);

        auto ubi_quantity = existing->quantity;
        auto ubi_amount = ubi_quantity.amount;
        auto eos_quantity = quantity;
        auto eos_amount = eos_quantity.amount;
        double price = double(ubi_balance) / eos_balance;

        uint64_t eos_add_amount, ubi_add_amount;
        // eos amount enough
        if (double(ubi_amount) / eos_amount <= price)
        {
          eos_add_amount = double(ubi_amount) / price;
          ubi_add_amount = ubi_amount;
        }
        else
        {
          eos_add_amount = eos_amount;
          ubi_add_amount = eos_amount * price;
        }
        uint64_t eos_back_amount = eos_amount - eos_add_amount;
        uint64_t ubi_back_amount = ubi_amount - ubi_add_amount;

        // transfer fund to store
        if (ubi_add_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              DAPP_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), STORE_CONTRACT, asset(ubi_add_amount, UBI_SYMBOL), std::string("Add liquidity to pool")))
              .send();
        }
        if (eos_add_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              SYSTEM_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), STORE_CONTRACT, asset(eos_add_amount, EOS_SYMBOL), std::string("Add liquidity to pool")))
              .send();
        }

        if (ubi_back_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              DAPP_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), from, asset(ubi_back_amount, UBI_SYMBOL), std::string("Change for Adding liquidity to pool")))
              .send();
        }
        if (eos_back_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              SYSTEM_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), from, asset(eos_back_amount, EOS_SYMBOL), std::string("Change for Adding liquidity to pool")))
              .send();
        }
        //      eosio::check(addresses.get_code() == "dan"_n, "Codes don't match.");
      }
      return;
    }

    //deduct fee
    double received = quantity.amount;
    received = received * (1 - FEE_RATE);

    double product = ubi_balance * eos_balance;

    double buy = ubi_balance - (product / (received + eos_balance));

    auto buy_quantity = asset(buy, UBI_SYMBOL);

    // transfer to user
    action(
        permission_level{STORE_CONTRACT, "active"_n},
        DAPP_TOKEN_CONTRACT,
        "transfer"_n,
        std::make_tuple(STORE_CONTRACT, from, buy_quantity, std::string("Buy UBI with EOS")))
        .send();

    // transfer fund to store
    action(
        permission_level{to, "active"_n},
        SYSTEM_TOKEN_CONTRACT,
        "transfer"_n,
        std::make_tuple(to, STORE_CONTRACT, quantity, std::string("Buy UBI with EOS")))
        .send();
  }
}

void tokenuniswap::receive_ubi(name from, name to, asset quantity, std::string memo)
{
  if (to == get_self() && quantity.symbol == UBI_SYMBOL)
  {
    // get EOS balance
    double eos_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, STORE_CONTRACT, EOS_SYMBOL.code()).amount;

    // get UBI balance
    double ubi_balance = eosio::token::get_balance(DAPP_TOKEN_CONTRACT, STORE_CONTRACT, UBI_SYMBOL.code()).amount;

    // add liquidity logic
    if (memo == "add")
    {
      liquidity_index liquidityRecords(get_self(), from.value);
      auto existing = liquidityRecords.find(SYSTEM_TOKEN_CONTRACT.value);
      // not exsit
      if (existing == liquidityRecords.end())
      {
        liquidityRecords.emplace(_self, [&](auto &record) {
          record.contract = DAPP_TOKEN_CONTRACT;
          record.quantity = quantity;
        });
      }
      else
      {
        liquidityRecords.erase(existing);

        auto ubi_quantity = quantity;
        auto ubi_amount = ubi_quantity.amount;
        auto eos_quantity = existing->quantity;
        auto eos_amount = eos_quantity.amount;
        double price = double(ubi_balance) / eos_balance;

        uint64_t eos_add_amount, ubi_add_amount;
        // eos amount enough
        if (double(ubi_amount) / eos_amount <= price)
        {
          eos_add_amount = double(ubi_amount) / price;
          ubi_add_amount = ubi_amount;
        }
        else
        {
          eos_add_amount = eos_amount;
          ubi_add_amount = eos_amount * price;
        }
        uint64_t eos_back_amount = eos_amount - eos_add_amount;
        uint64_t ubi_back_amount = ubi_amount - ubi_add_amount;

        // transfer fund to store
        if (ubi_add_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              DAPP_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), STORE_CONTRACT, asset(ubi_add_amount, UBI_SYMBOL), std::string("Add liquidity to pool")))
              .send();
        }
        if (eos_add_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              SYSTEM_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), STORE_CONTRACT, asset(eos_add_amount, EOS_SYMBOL), std::string("Add liquidity to pool")))
              .send();
        }

        if (ubi_back_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              DAPP_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), from, asset(ubi_back_amount, UBI_SYMBOL), std::string("Change for Adding liquidity to pool")))
              .send();
        }
        if (eos_back_amount > 0)
        {
          action(
              permission_level{get_self(), "active"_n},
              SYSTEM_TOKEN_CONTRACT,
              "transfer"_n,
              std::make_tuple(get_self(), from, asset(eos_back_amount, EOS_SYMBOL), std::string("Change for Adding liquidity to pool")))
              .send();
        }
        //      eosio::check(addresses.get_code() == "dan"_n, "Codes don't match.");
      }
      return;
    }

    //deduct fee
    double received = quantity.amount;
    received = received * (1 - FEE_RATE);

    double product = ubi_balance * eos_balance;

    double sell = eos_balance - (product / (received + ubi_balance));

    auto sell_quantity = asset(sell, EOS_SYMBOL);

    // transfer to user
    action(
        permission_level{STORE_CONTRACT, "active"_n},
        SYSTEM_TOKEN_CONTRACT,
        "transfer"_n,
        std::make_tuple(STORE_CONTRACT, from, sell_quantity, std::string("Sell UBI for EOS")))
        .send();

    // transfer fund to store
    action(
        permission_level{to, "active"_n},
        DAPP_TOKEN_CONTRACT,
        "transfer"_n,
        std::make_tuple(to, STORE_CONTRACT, quantity, std::string("Sell UBI for EOS")))
        .send();
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