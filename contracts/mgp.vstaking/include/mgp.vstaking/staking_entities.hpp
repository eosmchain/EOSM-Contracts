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


static constexpr eosio::name active_perm{"active"_n};
static constexpr eosio::name SYS_BANK{"eosio.token"_n};

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.vstaking")]]

struct CONTRACT_TBL configs_ {
    name account;
    string burn_memo;
    int destruction;
    bool redeemable;
    asset minpay;

    uint64_t primary_key() const { return account.value; }

    EOSLIB_SERIALIZE( configs_, (account)(burn_memo)(destruction)
                                (redeemable)(minpay) )
};
typedef eosio::multi_index<"configs"_n, configs_> configs;	


struct CONTRACT_TBL configs2_t {
    name account;
    bool data_correction_enabled = false;

    uint64_t primary_key() const { return account.value; }

    EOSLIB_SERIALIZE( configs2_t, (account)(data_correction_enabled) )
};
typedef eosio::multi_index<"configs.ext"_n, configs2_t> configs2_tbl;	

struct CONTRACT_TBL balances_ {
    name account;
    asset remaining;
    uint64_t primary_key() const { return account.value; }
};
typedef eosio::multi_index<"balances"_n, balances_> balances;

// 新增额度表
struct CONTRACT_TBL quota_ {
    name account;
    asset remaining;
    uint64_t primary_key() const { return account.value; }
};
typedef eosio::multi_index<"quota"_n, quota_> quota;
	
struct CONTRACT_TBL ethaddressbook_{
    name account;
    string address;

    uint64_t primary_key() const{return account.value;}

    EOSLIB_SERIALIZE( ethaddressbook_, (account)(address) )
};
typedef eosio::multi_index<"addressbook"_n,ethaddressbook_> addressbook;


}