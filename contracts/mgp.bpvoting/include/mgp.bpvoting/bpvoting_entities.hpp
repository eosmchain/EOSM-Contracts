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

static constexpr uint64_t BPVOTING_SCOPE        = 1000;
static constexpr uint32_t seconds_per_month     = 24 * 3600 * 30;
static constexpr uint32_t seconds_per_week      = 24 * 3600 * 7;
static constexpr uint32_t seconds_per_day       = 24 * 3600;
static constexpr uint32_t rewards_to_bp_per_day = 1580;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.bpvoting")]]

struct [[eosio::table("global"), eosio::contract("mgp.bpvoting")]] global_t {
    uint64_t max_iterate_steps_tally_vote    = 30;
    uint64_t max_iterate_steps_tally_unvote  = 20;
    uint64_t max_iterate_steps_reward        = 50;
    uint64_t max_bp_size;
    uint64_t max_candidate_size;
    uint64_t min_bp_list_amount;
    uint64_t min_bp_accept_amount;
    uint64_t refund_time;

    time_point_sec started_at;

    asset total_listed;
    asset total_staked;
    asset total_rewarded;

    global_t() {
        max_iterate_steps_tally_vote    = 30;
        max_iterate_steps_tally_unvote  = 20;
        max_iterate_steps_reward        = 50;

        max_bp_size             = 21;
        max_candidate_size      = 30;
        min_bp_list_amount      = 10'0000ll;
        min_bp_accept_amount    = 20'0000ll;
        refund_time             = 3 * 24 * 3600; //3-days in sec
    }

    EOSLIB_SERIALIZE( global_t, (max_iterate_steps_tally_vote)
                                (max_iterate_steps_tally_unvote)
                                (max_iterate_steps_reward)
                                (max_bp_size)(max_candidate_size)
                                (min_bp_list_amount)(min_bp_accept_amount)(refund_time) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

/**
 * election round table, one record per day
 *
 * for current onging round,
 */
struct CONTRACT_TBL election_round_t{
    uint64_t round_id;          //one day one round
    time_point_sec started_at;
    time_point_sec ended_at;

    uint64_t vote_count;
    uint64_t unvote_count;
    uint64_t vote_tallied_count;
    uint64_t unvote_tallied_count;
    
    bool     vote_tally_completed   = false;
    bool     unvote_tally_completed = false;
    bool     reward_completed       = false;
    bool     execute_completed      = false;

    std::map<name, asset> elected_bps;      //max 21 bps
    asset total_votes;
    asset total_votes_in_coinage;
    asset available_rewards;    //rewards from last inflation distribution
    asset total_rewards;        //total received accumualted rewards
 
    election_round_t() {}
    election_round_t(uint64_t rid): round_id(rid) {}

    uint64_t primary_key()const { return round_id; }
    uint64_t scope() const { return BPVOTING_SCOPE; }

    typedef eosio::multi_index<"candidates"_n, candidate_t> table_t;

    EOSLIB_SERIALIZE(election_round_t,  (round_id)(bps)(total_votes)
                                        (available_rewards)(total_rewards) )
};

// added by BP candidate
struct CONTRACT_TBL candidate_t {
    name owner;

    /// Upon BP rewarding, distribute partially to voters who vote for self
    int8_t self_reward_share; //boost by 10000
    asset staked_votes;     //self staked
    asset received_votes;   //other voted
    asset last_claimed_rewards;   //unclaimed total rewards
    asset total_claimed_rewards;  //unclaimed total rewards
    asset unclaimed_rewards;      //unclaimed total rewards

    candidate_t() {}
    candidate_t(const name& c): candidate(c) {}
  
    uint64_t primary_key() const { return owner.value; }
    uint64_t scope() const { return BPVOTING_SCOPE; }

    typedef eosio::multi_index<"candidates"_n, candidate_t> table_t;

    EOSLIB_SERIALIZE(candidate_t,   (owner)(self_reward_share)
                                    (staked_votes)(received_votes) )
};

/**
 *  vote table
 */
struct CONTRACT_TBL vote_t {
    name owner;         //voter
    name candidate;     //target candidiate to vote for
    asset quantity;
    time_point voted_at;
    time_point restarted_at;   //restart age counting every 30-days
    time_point last_vote_tallied_at;
    time_point last_unvote_tallied_at;
    time_point last_rewarded_at;

    double by_voted_at()                { return voted_at.sec_since_epoch();                }
    double by_restarted_at()            { return restarted_at.sec_since_epoch();            } 
    double by_last_vote_tallied_at()    { return last_vote_tallied_at.sec_since_epoch();    }
    double by_last_unvote_tallied_at()  { return last_unvote_tallied_at.sec_since_epoch();  }
    double by_last_rewarded_at()        { return last_rewarded_at.sec_since_epoch();        }

    uint64_t primary_key() { return owner.value; }

    vote_t() {}
    vote_t(const asset& q, time_point t): quantity(q), voted_at(t) {}

    EOSLIB_SERIALIZE( vote_t, (owner)(candidate)(quantity)(voted_at)(restarted_at)(last_tallied_at)(last_rewarded_at) )
};
typedef eosio::multi_index< "votes"_n, vote_t,
                            indexed_by<"voteda"_n, const_mem_fun<vote_t, double, &vote_t::by_voted_at> >,
                            indexed_by<"restarted"_n, const_mem_fun<vote_t, double, &vote_t::by_last_restarted_at> >,
                            indexed_by<"lvotallied"_n, const_mem_fun<vote_t, double, &vote_t::by_last_vote_tallied_at> >,
                            indexed_by<"luvtallied"_n, const_mem_fun<vote_t, double, &vote_t::by_last_unvote_tallied_at> >,
                            indexed_by<"lastrewarded"_n, const_mem_fun<vote_t, double, &vote_t::by_last_rewarded_at> >
                             > vote_tbl;

struct CONTRACT_TBL unvote_t {
    name owner;         //voter
    name candidate;     //target candidiate to unvote for
    asset quantity;
    time_point unvoted_at;
    time_point last_tallied_at;

    double by_unvoted_at()        { return unvoted_at.sec_since_epoch();          }
    double by_last_tallied_at()   { return last_tallied_at.sec_since_epoch();     }

    uint64_t primary_key() { return owner.value; }

    unvote_t() {}
    unvote_t(const asset& q, time_point t): quantity(q), unvoted_at(t) {}

    EOSLIB_SERIALIZE( unvote_t, (owner)(candidate)(quantity)(unvoted_at)(last_tallied_at) )
};
typedef eosio::multi_index< "unvotes"_n, unvote_t,
                            indexed_by<"unvoteda"_n, const_mem_fun<vote_t, double, &vote_t::by_unvoted_at> >,
                            indexed_by<"lasttallied"_n, const_mem_fun<vote_t, double, &vote_t::by_last_tallied_at> >,
                             > unvote_tbl;

struct CONTRACT_TBL voter_t {
    name                owner;                  //the voter
    asset               total_staked;           //sum of all votes
    asset               last_claimed_rewards;   //unclaimed total rewards
    asset               total_claimed_rewards;  //unclaimed total rewards
    asset               unclaimed_rewards;      //unclaimed total rewards

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