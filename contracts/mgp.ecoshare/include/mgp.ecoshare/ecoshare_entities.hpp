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

static constexpr eosio::name T_COUNTER{"transfer"_n};
static constexpr eosio::name SYS_BANK{"eosio.token"_n};
static constexpr symbol SYS_SYMBOL = symbol(symbol_code("MGP"), 4);

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.ecoshare")]]

struct [[eosio::table("global"), eosio::contract("mgp.ecoshare")]] global_tbl {
    uint64_t bps_voting_share   = 2000; //20% * 10000
    name bps_voting_account     = "mgp.bpvoting"_n;
    name stake_mining_account   = "mgpchain2222"_n;
    uint64_t last_transfer_id   = 0;

    global_tbl(){}

    EOSLIB_SERIALIZE( global_tbl, (bps_voting_share)(bps_voting_account)(stake_mining_account)(last_transfer_id) )
};
typedef eosio::singleton< "global"_n, global_tbl > global_singleton;


struct CONTRACT_TBL transfer_t {
    uint64_t id;

    name bps_voting_account;
    name stake_mining_account;
    asset bps_voting_transferred    = asset(0, SYS_SYMBOL);
    asset stake_mining_transferred  = asset(0, SYS_SYMBOL);
    time_point transferred_at;

    transfer_t() {}
    transfer_t(name code, uint64_t scope) {
        index_t tbl(code, scope);
        id = tbl.available_primary_key();
    }
    transfer_t(const uint64_t tid): id(tid){}

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }
    
    typedef eosio::multi_index<"transfers"_n, transfer_t> index_t;

    EOSLIB_SERIALIZE( transfer_t,   (id)(bps_voting_account)(stake_mining_account)
                                    (bps_voting_transferred)(stake_mining_transferred)
                                    (transferred_at) )
};


}