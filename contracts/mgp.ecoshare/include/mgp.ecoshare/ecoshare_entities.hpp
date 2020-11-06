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

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.ecoshare")]]

struct [[eosio::table("global"), eosio::contract("mgp.ecoshare")]] global_tbl {
    uint64_t bps_voting_share = 2000; //20% * 10000
    name bps_voting_account = "mgp.bpsvoting"_n;
    name stake_mining_account = "addressbookt"_n;

    global_tbl(){}

    EOSLIB_SERIALIZE( global_tbl, (bps_voting_share)(bps_voting_account)(stake_mining_account) )
};
typedef eosio::singleton< "global"_n, global_tbl > global_singleton;


struct CONTRACT_TBL transfer_t {
    uint64_t transfer_id;

    name bps_voting_account;
    name stake_mining_account;
    asset bps_voting_share;
    asset stake_mining_share;

    transfer_t() {}
    transfer_t(const uint64_t tid): transfer_id(tid){}
    transfer_t(const uint64_t tid, const name& bva, const name& sma, 
                const asset& bvs, const asset& sms): transfer_id(tid),
                bps_voting_account(bva), stake_mining_account(sma), 
                bps_voting_share(bvs), stake_mining_share(sms) {}
    

    uint64_t primary_key() const { return transfer_id; }
    typedef eosio::multi_index<"transfers"_n, transfer_t> table_t;	

    EOSLIB_SERIALIZE( transfer_t,   (transfer_id)(bps_voting_account)(stake_mining_account)
                                    (bps_voting_share)(stake_mining_share) )
};


struct CONTRACT_TBL counter_t {
    name            counter_key;    //stake_counter | admin_counter
    uint64_t        counter_val;

    uint64_t primary_key() const { return counter_key.value; }
    uint64_t scope() const { return ECOSHARE_SCOPE; }

    counter_t() {}
    counter_t(name counterKey): counter_key(counterKey) {}

    typedef eosio::multi_index<"counters"_n, counter_t> table_t;

    EOSLIB_SERIALIZE( counter_t, (counter_key)(counter_val) )
};

}