#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include "wasm_db.hpp"
#include "bpvoting_entities.hpp"

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

class [[eosio::contract("mgp.bpvoting")]] mgp_bpvoting: public eosio::contract {
  private:
    global_singleton    _global;
    global_t            _gstate;
    dbc                 _dbc;

  public:
    using contract::contract;
    mgp_bpvoting(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value), _dbc(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~mgp_bpvoting() {
        _global.set( _gstate, get_self() );
    }

    [[eosio::action]]
    void withdraw(const std::vector<name>& admins);


  private:
    void process_vote(const std::vector<name>& admins);

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


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
	if ( code == SYS_BANK.value && action == "transfer"_n.value) {
		eosio::execute_action(  eosio::name(receiver), 
                            eosio::name(code), 
                            &mgp_ecoshare::transfer );

	} else if (code == receiver) {
    // check( false, "none action to invoke!" );

		switch (action) {
			EOSIO_DISPATCH_HELPER( mgp_ecoshare, (config)(withdraw))
		}
	}
}

}