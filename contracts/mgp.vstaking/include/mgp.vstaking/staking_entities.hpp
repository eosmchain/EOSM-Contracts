 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <deque>
#include <optional>
#include <string>
#include <type_traits>

namespace mgp {

using namespace std;
using namespace eosio;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.vstaking")]]

struct CONTRACT_TBL configs_t {
    name account;
    string burn_memo;
    int destruction;
    bool redeemallow;
    asset minpay;

    configs_t() {}
    configs_t(const name& a): account(a){}

    uint64_t primary_key() const { return account.value; }

    typedef eosio::multi_index<"configs"_n, configs_t> table_t;	

    EOSLIB_SERIALIZE( configs_t, (account)(burn_memo)(destruction)(redeemallow)(minpay) ) 
};

struct CONTRACT_TBL configs2_t {
    name account;
    bool data_correction_enabled = false;

    configs2_t() {}
    configs2_t(const name& a): account(a){}

    uint64_t primary_key() const { return account.value; }
    typedef eosio::multi_index<"configs.ext"_n, configs2_t> table_t;	

    EOSLIB_SERIALIZE( configs2_t, (account)(data_correction_enabled) )
};

struct CONTRACT_TBL balances_t {
    name account;
    asset remaining;

    balances_t() {}
    balances_t(const name& a): account(a) {}

    uint64_t primary_key() const { return account.value; }
    
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

    typedef eosio::multi_index<"addressbook"_n, ethaddressbook_t> table_t;

    EOSLIB_SERIALIZE( ethaddressbook_t, (account)(address) )
};


}