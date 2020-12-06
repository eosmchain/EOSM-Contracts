#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include "wasm_db.hpp"
#include "ecoshare_entities.hpp"

namespace mgp {

using eosio::asset;
using eosio::check;
using eosio::datastream;
using eosio::name;
using eosio::symbol;
using eosio::symbol_code;
using eosio::unsigned_int;

using std::string;
using namespace wasm::db;

class [[eosio::contract("mgp.ecoshare")]] mgp_ecoshare: public eosio::contract {
  private:
    global_singleton    _global;
    global_tbl          _gstate;
    dbc                 _dbc;

  public:
    using contract::contract;
    mgp_ecoshare(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value), _dbc(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_tbl{};
    }

    ~mgp_ecoshare() 
    {
        _global.set( _gstate, get_self() );
    }

    [[eosio::action]]
    void init();  //only code maintainer can init

    [[eosio::action]]
    void config(const uint64_t bps_voting_share,
              const name& bps_voting_account,
              const name& stake_mining_account);

    // [[eosio::action]]
    // void withdraw(const asset& quant);

    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(name from, name to, asset quantity, string memo);

    using init_action     = action_wrapper<name("init"),      &mgp_ecoshare::init     >;
    using config_action   = action_wrapper<name("config"),    &mgp_ecoshare::config   >;
    using transfer_action = action_wrapper<name("transfer"),  &mgp_ecoshare::deposit  >;

};

}