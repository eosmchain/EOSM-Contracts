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

#define VSTAKING_TBL [[eosio::table, eosio::contract("mgp.vstaking")]]

struct VSTAKING_TBL balances_ {
    name account;
    asset remaining;
    uint64_t primary_key() const { return account.value; }
};
typedef eosio::multi_index<"balances"_n, balances_> balances;


}