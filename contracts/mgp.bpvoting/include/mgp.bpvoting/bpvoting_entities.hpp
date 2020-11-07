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

    set<name> bps;          //list of elected bp accounts, max 21, updated every hour

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
 * 21 elected bps, sorted by total_votes
 */
struct CONTRACT_TBL vbp_t{
    name account;
    asset total_votes;

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


/**
 *  vote info
 */
struct vote_info {
    asset quantity;
    time_point_sec voted_at;

    vote_info() {}
    vote_info(const asset& q, time_point t): quantity(q), voted_at(t) {}

    EOSLIB_SERIALIZE( vote_info, (quantity)(voted_at) )
};

struct CONTRACT_TBL voter_t {
    name                owner;                  //the voter
    asset               total_staked;           //sum of all votes
    asset               last_claimed_rewards;   //unclaimed total rewards
    asset               total_claimed_rewards;  //unclaimed total rewards
    asset               unclaimed_rewards;      //unclaimed total rewards
    std::map<name, 
            vote_info>  votes;                  //candidate -> vote_info, max 30 candidates

    voter_t() {}
    voter_t(const name& o): owner(o) {}

    uint64_t primary_key() const { return owner.value; }
    uint64_t scope() const { return BPVOTING_SCOPE; }

    typedef eosio::multi_index<"voters"_n, voter_t> table_t;

    EOSLIB_SERIALIZE( voter_t,  (owner)(total_staked)
                                (last_claimed_rewards)(total_claimed_rewards)(unclaimed_rewards) 
                                (votes) )
};


}