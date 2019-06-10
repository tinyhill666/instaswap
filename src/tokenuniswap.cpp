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
    double eos_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, get_self(), EOS_SYMBOL.code()).amount;

    double received = quantity.amount;

    // get UBI balance
    double ubi_balance = eosio::token::get_balance(DAPP_TOKEN_CONTRACT, get_self(), UBI_SYMBOL.code()).amount;

    //deduct fee
    received = received * (1 - FEE_RATE);

    double product = ubi_balance * eos_balance;

    double buy = ubi_balance - (product / (received + eos_balance));

    auto buy_quantity = asset(buy, UBI_SYMBOL);

    action(
        permission_level{to, "active"_n},
        DAPP_TOKEN_CONTRACT,
        "transfer"_n,
        std::make_tuple(to, from, buy_quantity, std::string("Buy UBI with EOS")))
        .send();
  }
}

void tokenuniswap::receive_ubi(name from, name to, asset quantity, std::string memo)
{
  if (to == get_self() && quantity.symbol == UBI_SYMBOL)
  {
    // get EOS balance
    double eos_balance = eosio::token::get_balance(SYSTEM_TOKEN_CONTRACT, get_self(), EOS_SYMBOL.code()).amount;

    double received = quantity.amount;

    // get UBI balance
    double ubi_balance = eosio::token::get_balance(DAPP_TOKEN_CONTRACT, get_self(), UBI_SYMBOL.code()).amount;

    //deduct fee
    received = received * (1 - FEE_RATE);

    double product = ubi_balance * eos_balance;

    double sell = eos_balance - (product / (received + ubi_balance));

    auto sell_quantity = asset(sell, EOS_SYMBOL);

    action(
        permission_level{to, "active"_n},
        SYSTEM_TOKEN_CONTRACT,
        "transfer"_n,
        std::make_tuple(to, from, sell_quantity, std::string("Sell UBI for EOS")))
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