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

#define CONTRACT_TBL [[eosio::table, eosio::contract("adddressbookt")]]


struct [[eosio::table("configs"), eosio::contract("adddressbookt")]] mgp_global_state {
    name account;
    string burn_memo;
    int destruction;
    bool redeemallow;
    asset minpay;

    mgp_global_state() {}
    // uint64_t primary_key() const { return account.value; }

    EOSLIB_SERIALIZE( mgp_global_state, (account)(burn_memo)(destruction)(redeemallow)(minpay) ) 
};
// typedef eosio::multi_index<"configs"_n, user_config_t> user_config_tbl;	
typedef eosio::singleton< "configs"_n, mgp_global_state > global_state_singleton;

struct [[eosio::table("configs2"), eosio::contract("adddressbookt")]] mgp_global_state2 {
    bool data_correction_enabled = false;

    mgp_global_state2() {}

    EOSLIB_SERIALIZE( mgp_global_state2, (data_correction_enabled) )
};
typedef eosio::singleton< "configs2"_n, mgp_global_state2 > global_state2_singleton;

struct CONTRACT_TBL balances_t {
    name account;
    asset remaining;

    balances_t() {}
    balances_t(const name& a): account(a) {}

    uint64_t primary_key() const { return account.value; }
    uint64_t scope() const { return ADDRESSBOOK_SCOPE; }
    
    typedef eosio::multi_index<"balances"_n, balances_t> table_t;

    EOSLIB_SERIALIZE( balances_t, (account)(remaining) )
};


// 新增额度表
// struct CONTRACT_TBL quota_t {
//     name account;
//     asset remaining;

//     quota_t() {}
//     quota_t(const name& a): account(a) {}
//     uint64_t primary_key() const { return account.value; }

//     typedef eosio::multi_index<"quota"_n, quota_t> table_t;
// };


struct CONTRACT_TBL ethaddressbook_t {
    name account;
    string address;

    ethaddressbook_t() {}
    ethaddressbook_t(const name& acct): account(acct) {}

    uint64_t primary_key() const{ return account.value; }
    uint64_t scope() const { return ADDRESSBOOK_SCOPE; }

    typedef eosio::multi_index<"addressbook"_n, ethaddressbook_t> table_t;

    EOSLIB_SERIALIZE( ethaddressbook_t, (account)(address) )
};



class [[eosio::contract("addressbookt")]] smart_mgp: public eosio::contract {
  private:
    global_state_singleton  _global;
    global_state2_singleton _global2;
    mgp_global_state        _gstate;
    mgp_global_state2       _gstate2;

    dbc                     _dbc;

  public:
    using contract::contract;
	smart_mgp(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), 
        _global(get_self(), get_self().value), 
        _global2(get_self(), get_self().value),
        _dbc(get_self())
    {
        _gstate = _global.exists() ? _global.get() : get_default_global();
        _gstate2 = _global2.exists() ? _global2.get() : mgp_global_state2{};
    }

    ~smart_mgp() {
        _global.set( _gstate, _self );
        _global2.set( _gstate2, _self );
    }

    mgp_global_state get_default_global() {
        mgp_global_state gs;

        gs.account      = _self;
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