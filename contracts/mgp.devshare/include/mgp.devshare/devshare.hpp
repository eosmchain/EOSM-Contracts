#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include "devshare_db.hpp"
#include "devshare_entities.hpp"

using namespace wasm::db;

namespace mgp {

using eosio::asset;
using eosio::check;
using eosio::datastream;
using eosio::name;
using eosio::symbol;
using eosio::symbol_code;
using eosio::unsigned_int;

using std::string;

static constexpr eosio::name P_COUNTER{"proposal"_n};
static constexpr eosio::name SYS_BANK{"eosio.token"_n};
static constexpr symbol SYS_SYMBOL = symbol(symbol_code("MGP"), 4);

class [[eosio::contract("mgp.devshare")]] mgp_devshare: public eosio::contract {
  private:
    global_singleton    _global;
    global_tbl          _gstate;
    dbc                 _dbc;

  public:
    using contract::contract;
    mgp_devshare(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value), _dbc(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_tbl{};
    }

    ~mgp_devshare() {
        _global.set( _gstate, get_self() );
    }

    [[eosio::action]]
    void addadmins(const std::vector<name>& admins);

    [[eosio::action]]
    void deladmins(const std::vector<name>& admins);

    [[eosio::action]]
    void propose(const name& issuer, uint64_t proposal_id, const asset& withdraw_quantity);

    [[eosio::action]]
    void approve(const name& issuer, uint64_t proposal_id);

    [[eosio::action]]
    void execute(const name& issuer, uint64_t proposal_id);

  private:
    uint64_t gen_new_id(const name &counter_key) {
        uint64_t newID = 1;
        counter_t counter(counter_key);
        if (!_dbc.get(counter)) {
            counter.counter_val = 1;
            _dbc.set(counter);

            return 1;
        }

        counter.counter_val++;
        _dbc.set(counter);

        return counter.counter_val;
    }


};

}