#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>


namespace ecoshare {


#define BASE_SYMBOL symbol("MGP", 4)
#define BASE_TRANSFER_FROM "eosio.token"_n
#define SYS_ACCOUNT "mgpchain2222"_n
#define SHOP_ACCOUNT "mgpchainshop"_n

CONTRACT mgp_ecoshare : public contract {
  public:
    using contract::contract;
    mgp_ecoshare(eosio::name receiver, eosio::name code, datastream<const char*> ds):contract(receiver, code, ds) {}
	
	TABLE configs_ {
        name account;
        string burn_memo;
        int destruction;
        bool redeemallow;
        asset minpay;
		uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"configs"_n, configs_> configs;	
	
	TABLE balances_ {
        name account;
        asset remaining;
		uint64_t primary_key() const { return account.value; }
    };
	
	// 新增额度表
	TABLE quota_ {
        name account;
        asset remaining;
		uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"quota"_n, quota_> quota;
	
	TABLE ethaddressbook_{
        name account;
        string address;
        uint64_t primary_key() const{return account.value;}
    };
    typedef eosio::multi_index<"addressbook"_n,ethaddressbook_> addressbook;

	ACTION configure( string burn_memo, int destruction, bool redeemallow, asset minpay );
	ACTION redeem( name account );
    ACTION bindaddress(name account,string address);
    ACTION delbind(name account,string address);
	
	void transfer( name from, name to, asset quantity, string memo );
};



}