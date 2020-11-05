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

struct [[eosio::table("global"), eosio::contract("mgp.bpvoting")]] global_t {
    global_t(){
        max_bp_size = 21; 
        min_bp_list_amount = 10'0000ll;   
        min_bp_accept_amount = 20'0000ll;   
        refund_time = 3 * 24 * 3600; //3-days in sec
    }

    uint64_t max_bp_size;
    uint64_t min_bp_stake_amount;
    uint64_t refund_time;

    EOSLIB_SERIALIZE( global_t, (max_bp_size)
                                (min_bp_list_amount)(min_bp_accept_amount)
                                (refund_time) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct CONTRACT_TBL candidate_t {
    name candidate;
    asset staked_votes; //listing staked
    asset total_votes;  //including staked_votes
    int8_t voter_share_percent; //boost by 100
    typedef eosio::multi_index< "producers"_n, producer_info,
                            indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
                            > producers_table;

};



struct CONTRACT_TBL vote_t {
    name voter;
    asset votes;

    vote_t(){}
    proposal_t(uint64_t pid): proposal_id(pid) {}
    proposal_t(uint64_t pid, name p, asset a, time_point t): 
        proposal_id(pid), proposer(p), propose_to_withdraw(a), expired_at(t) {}

    uint64_t primary_key()const { return proposal_id; }     
    uint64_t scope() const { return BPVOTING_SCOPE; }

    typedef eosio::multi_index<"votes"_n, vote_t> table_t;

    EOSLIB_SERIALIZE( proposal_t, (proposer)(propose_to_withdraw)(expired_at) )
};

struct CONTRACT_TBL counter_t {
    name            counter_key;    //stake_counter | admin_counter
    uint64_t        counter_val;

    uint64_t primary_key() const { return counter_key.value; }
    uint64_t scope() const { return BPVOTING_SCOPE; }

    counter_t() {}
    counter_t(name counterKey): counter_key(counterKey) {}

    typedef eosio::multi_index<"counters"_n, counter_t> table_t;

    EOSLIB_SERIALIZE( counter_t, (counter_key)(counter_val) )
};

}