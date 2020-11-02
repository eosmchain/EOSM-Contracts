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

static constexpr uint64_t BPVOTING_SCOPE = 1000;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.bpvoting")]]

struct CONTRACT_TBL global_t {
    global_t(){}

    uint64_t proposal_expire_in_hours = 24; //24 hours
    uint8_t max_member_size = 5;
    uint8_t min_approval_size = 4;
    std::set<name> devshare_members;

    EOSLIB_SERIALIZE( global_t, (proposal_expire_in_hours)(devshare_members) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct CONTRACT_TBL proposal_t {
    uint64_t proposal_id;
    name proposer;
    asset propose_to_withdraw;
    time_point expired_at;   //expire in 24 hours
    
    std::set<name> approval_members;

    proposal_t(){}
    proposal_t(uint64_t pid): proposal_id(pid) {}
    proposal_t(uint64_t pid, name p, asset a, time_point t): 
        proposal_id(pid), proposer(p), propose_to_withdraw(a), expired_at(t) {}

    uint64_t primary_key()const { return proposal_id; }     
    uint64_t scope() const { return DEVSHARE_SCOPE; }

    typedef eosio::multi_index<"proposals"_n, proposal_t> table_t;

    EOSLIB_SERIALIZE( proposal_t, (proposer)(propose_to_withdraw)(expired_at) )
};

struct CONTRACT_TBL counter_t {
    name            counter_key;    //stake_counter | admin_counter
    uint64_t        counter_val;

    uint64_t primary_key() const { return counter_key.value; }
    uint64_t scope() const { return DEVSHARE_SCOPE; }

    counter_t() {}
    counter_t(name counterKey): counter_key(counterKey) {}

    typedef eosio::multi_index<"counters"_n, counter_t> table_t;

    EOSLIB_SERIALIZE( counter_t, (counter_key)(counter_val) )
};

}