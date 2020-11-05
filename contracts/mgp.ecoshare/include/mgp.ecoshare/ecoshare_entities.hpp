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

using namespace eosio;

static constexpr uint64_t ECOSHARE_SCOPE = 1000;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.devshare")]]

struct [[eosio::table("global"), eosio::contract("mgp.ecoshare")]] global_tbl {
    uint64_t bps_voting_share = 2000; //20% * 10000
    name bps_voting_account = "mgp.bpsvoting"_n;
    name stake_mining_account = "addressbookt"_n;

    global_tbl(){}

    EOSLIB_SERIALIZE( global_tbl, (bps_voting_share)(bps_voting_account)(stake_mining_account) )
};
typedef eosio::singleton< "global"_n, global_tbl > global_singleton;


}