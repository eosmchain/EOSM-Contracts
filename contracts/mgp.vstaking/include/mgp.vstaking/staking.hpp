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
static constexpr eosio::name MASTER_ACCOUNT{"masteraychen"_n};

static constexpr symbol SYS_SYMBOL = symbol(symbol_code("MGP"), 4);
static constexpr uint64_t ADDRESSBOOK_SCOPE = 1000;

class [[eosio::contract("mgp.vstaking")]]  smart_mgp: public eosio::contract {
  public:
    using contract::contract;

  	ACTION configure( string burn_memo, int destruction, bool redeemallow, asset minpay );
    ACTION encorrection( bool enable_data_correction );
    ACTION bindaddress(const name& account, const string& address);
    ACTION delbind(const name& account, const string& address);
    ACTION redeem(name issuer);
    ACTION reloadnum(const name& from, const name& to, const asset& quant);
	
    /// NOTE: 
    /// DO NOT CHANGE PARAM TYPES OTHERWISE CAUSING DATA LOADING ISSUES in EOSWEB
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