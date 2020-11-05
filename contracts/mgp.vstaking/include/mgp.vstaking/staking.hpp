#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include <deque>
#include <optional>
#include <type_traits>

#include "wasm_db.hpp"
#include "staking_entities.hpp"

namespace mgp {

using namespace wasm::db;

using eosio::asset;
using eosio::check;
using eosio::datastream;
using eosio::name;
using eosio::symbol;
using eosio::symbol_code;
using eosio::unsigned_int;

using std::string;

static constexpr eosio::name SYS_ACCOUNT{"mgpchain2222"_n};
static constexpr eosio::name SHOP_ACCOUNT{"mgpchainshop"_n};
static constexpr eosio::name AGENT_ACCOUNT{"mgpagentdiya"_n};
static constexpr eosio::name SYS_BANK{"eosio.token"_n};
static constexpr symbol SYS_SYMBOL = symbol(symbol_code("MGP"), 4);
static constexpr uint64_t ADDRESSBOOK_SCOPE = 1000;

class [[eosio::contract("mgp.vstaking")]]  smart_mgp: public eosio::contract {
  private:
    configs_t               _configs;

    global_state_singleton  _configs2;
    mgp_global_state        _gstate2;

    dbc                     _dbc;

  public:
    using contract::contract;

	smart_mgp(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), 
        _configs2(get_self(), get_self().value),
        _dbc(get_self())
    {
        configs_t _configs(_self);	
        if (!_dbc.get(_configs))
            _configs = get_default_configs();

        _gstate2 = _configs2.exists() ? _configs2.get() : mgp_global_state{};
    }

    ~smart_mgp() {
        _dbc.set(_configs);

        _configs2.set( _gstate2, _self );
    }

    configs_t get_default_configs() {
        configs_t gs(_self);

        gs.burn_memo    = "destruction";
        gs.destruction  = 50;
        gs.redeemallow  = 0;
        gs.minpay       = asset(200'0000, SYS_SYMBOL);

        return gs;
    }

	ACTION configure( string burn_memo, int destruction, bool redeemallow, asset minpay );
    ACTION encorrection( bool enable_data_correction );
    ACTION bindaddress(const name& account, const string& address);
    ACTION delbind(const name& account, const string& address);
    ACTION redeem(const name& issuer);
    ACTION reloadnum(const name& from, const name& to, const asset& quant);
	
	void transfer(name from, name to, asset quantity, string memo);
};


extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action) {
	if ( code == SYS_BANK.value && action == "transfer"_n.value) {
		eosio::execute_action(  eosio::name(receiver), 
                                eosio::name(code), 
                                &smart_mgp::transfer );

	} else if (code == receiver) {
		switch (action) {
			EOSIO_DISPATCH_HELPER( smart_mgp,   (configure)(encorrection)
                                                (redeem)(bindaddress)(delbind)(reloadnum) )
		}
	}
}


} //end of namespace mgpecoshare