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

static constexpr uint64_t BPVOTING_SCOPE = 1000;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.bpvoting")]]

struct [[eosio::table("global"), eosio::contract("mgp.bpvoting")]] global_t {
    uint64_t max_bp_size;
    uint64_t max_candidate_size;
    uint64_t min_bp_list_amount;
    uint64_t min_bp_accept_amount;
    uint64_t refund_time;

    asset total_listed;
    asset total_staked;
    asset total_rewarded;

    global_t() {
        max_bp_size             = 21;
        max_candidate_size      = 30;
        min_bp_list_amount      = 10'0000ll;
        min_bp_accept_amount    = 20'0000ll;
        refund_time             = 3 * 24 * 3600; //3-days in sec
    }

    EOSLIB_SERIALIZE( global_t, (max_bp_size)
                                (min_bp_list_amount)(min_bp_accept_amount)(refund_time)
                                (bps) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

/**
 * election round table, one record per day
 *
 * for current onging round,
 */
struct CONTRACT_TBL election_round_t{
    uint64_t round_id;          //corresponding to days since the very 1st vote
    set<name> bps;
    asset total_votes;
    asset available_rewards;    //rewards from last inflation distribution
    asset total_rewards;        //total received accumualted rewards


    uint64_t primary_key()const { return account.value; }

    vbp_t() {}
    // typedef eosio::multi_index< "bps"_n, bp_t,
    //                         indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
    //                         > table_t;

    EOSLIB_SERIALIZE(vbp_t,   (account)(total_votes) )
};

// added by BP candidate
struct CONTRACT_TBL candidate_t {
    name candidate;

    /// Upon BP rewarding, distribute partially to
    /// voters who vote for self
    int8_t voter_reward_share_percent; //boost by 100

    asset staked_votes;     //self staked
    asset received_votes;   //other voted

    candidate_t() {}
    candidate_t(const name& c): candidate(c) {}
    // typedef eosio::multi_index< "candidates"_n, candidate_t,
    //                         indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
    //                         > table_t;

    uint64_t primary_key() const { return candidate.value; }
    uint64_t scope() const { return BPVOTING_SCOPE; }

    typedef eosio::multi_index<"candidates"_n, candidate_t> table_t;

    EOSLIB_SERIALIZE(candidate_t,   (candidate)(voter_reward_share_percent)
                                    (staked_votes)(received_votes) )
};

struct CONTRACT_TBL vote_t {
    name            owner;
    name            candidate;
    asset           voted;
    asset           total_rewarded;

    time_point_sec  voted_at;
    time_point_sec  updated_at; /* 0 - 24 h: days = 0
                                 * 1 - 30 days: counted as days,
                                 * 30+ days: days % 30
                                 */


};

/**
 *  vote info
 */
struct CONTRACT_TBL vote_t {
    name owner;         //voter
    name candidate;     //target candidiate to vote for
    asset quantity;
    block_timestamp voted_at;
    block_timestamp tallied_at;
    block_timestamp rewarded_at;

    uint64_t primary_key() { return owner.value; }

    vote_t() {}
    vote_t(const asset& q, time_point t): quantity(q), voted_at(t) {}

    EOSLIB_SERIALIZE( vote_t, (owner)(candidate)(quantity)(voted_at)(tallied_at)(rewarded_at) )
};

struct CONTRACT_TBL voter_t {
    name                owner;                  //the voter
    asset               total_staked;           //sum of all votes
    asset               last_claimed_rewards;   //unclaimed total rewards
    asset               total_claimed_rewards;  //unclaimed total rewards
    asset               unclaimed_rewards;      //unclaimed total rewards
    std::map<name,uint64_t>  votes;             //candidate -> vote_info, max 30 candidates

    voter_t() {}
    voter_t(const name& o): owner(o) {}

    uint64_t primary_key() const { return owner.value; }
    uint64_t scope() const { return BPVOTING_SCOPE; }

    typedef eosio::multi_index<"voters"_n, voter_t> table_t;

    EOSLIB_SERIALIZE( voter_t,  (owner)(total_staked)
                                (last_claimed_rewards)(total_claimed_rewards)(unclaimed_rewards)
                                (votes) )
};

/**
 *  Incoming rewards for whole bpvoting cohort
 *
 */
truct CONTRACT_TBL reward_t {
    asset quantity;
    time_point_sec rewarded_at;

    reward_t() {}
    uint64_t primary_key() const { return rewarded_at.sec_since_epoch(); }
};

}