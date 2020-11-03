#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

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

static constexpr eosio::name SYS_BANK{"eosio.token"_n};
static constexpr symbol SYS_SYMBOL = symbol(symbol_code("MGP"), 4);

class [[eosio::contract("mgp.ecoshare")]] mgp_ecoshare: public eosio::contract {
  private:
    global_singleton    _global;
    global_tbl          _gstate;

  public:
    using contract::contract;
    mgp_ecoshare(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_tbl{};
    }

    ~mgp_ecoshare() {
        _global.set( _gstate, get_self() );
    }

    [[eosio::action]]
    void config(const uint64_t bps_voting_share,
              const name& bps_voting_account,
              const name& stake_mining_account);

    [[eosio::action]]
    void withdraw(const asset& quant);

    void transfer(name from, name to, asset quantity, string memo);

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