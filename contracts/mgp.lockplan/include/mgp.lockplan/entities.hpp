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
static constexpr symbol   SYS_SYMBOL            = symbol(symbol_code("MGP"), 4);
static constexpr uint32_t seconds_per_year      = 24 * 3600 * 7 * 52;
static constexpr uint32_t seconds_per_month     = 24 * 3600 * 30;
static constexpr uint32_t seconds_per_week      = 24 * 3600 * 7;
static constexpr uint32_t seconds_per_day       = 24 * 3600;
static constexpr uint32_t seconds_per_hour      = 3600;
static constexpr uint32_t share_boost           = 10000;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.bpvoting")]]

struct [[eosio::table("global"), eosio::contract("mgp.bpvoting")]] global_t {
    uint64_t max_tally_vote_iterate_steps;
    uint64_t max_tally_unvote_iterate_steps;
    uint64_t max_reward_iterate_steps;
    uint64_t max_bp_size;
    uint64_t election_round_sec;                 
    uint64_t refund_delay_sec;
    uint64_t election_round_start_hour;     //GMT+0 Time
    asset min_bp_list_quantity;
    asset min_bp_accept_quantity;
    asset min_bp_vote_quantity;
    asset total_listed;
    asset total_voted;
    asset total_rewarded;
    time_point started_at;
    uint64_t last_election_round;   // most recently added
    uint64_t last_execution_round;  // most recently finished

    global_t() {
        max_tally_vote_iterate_steps    = 30;
        max_tally_unvote_iterate_steps  = 20;
        max_reward_iterate_steps        = 50;
        max_bp_size                     = 21;
        election_round_sec              = seconds_per_day;
        refund_delay_sec                = 3 * seconds_per_day;
        election_round_start_hour       = 1; //i.e. 9 AM for GMT+8 Shanghai Time, 24hrs per round
        min_bp_list_quantity            = asset(100'000'0000ll, SYS_SYMBOL);
        min_bp_accept_quantity          = asset(200'000'0000ll, SYS_SYMBOL);
        min_bp_vote_quantity            = asset(10'0000ll, SYS_SYMBOL); //10 MGP at least!
        total_listed                    = asset(0, SYS_SYMBOL);
        total_voted                     = asset(0, SYS_SYMBOL);
        total_rewarded                  = asset(0, SYS_SYMBOL);
    }

    EOSLIB_SERIALIZE( global_t, (max_tally_vote_iterate_steps)(max_tally_unvote_iterate_steps)
                                (max_reward_iterate_steps)(max_bp_size)
                                (election_round_sec)(refund_delay_sec)(election_round_start_hour)
                                (min_bp_list_quantity)(min_bp_accept_quantity)(min_bp_vote_quantity)
                                (total_listed)(total_voted)(total_rewarded)
                                (started_at)(last_election_round)(last_execution_round) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct [[eosio::table("global2"), eosio::contract("mgp.bpvoting")]] global2_t {
    uint64_t last_vote_tally_index = 0;  //always grow, round by round
    uint64_t vote_reward_index = 0; //always reset to 0 after finishing at each reward round
};
typedef eosio::singleton< "global2"_n, global2_t > global2_singleton;

struct bp_info_t {
    asset total_votes               = asset(0, SYS_SYMBOL);
    asset allocated_bp_rewards      = asset(0, SYS_SYMBOL); //must be reset upon execution
    asset allocated_voter_rewards   = asset(0, SYS_SYMBOL); //must be reset upon execution

    bp_info_t() {}
    bp_info_t(const asset& tv, const asset& abr, const asset& avr):
        total_votes(tv), allocated_bp_rewards(abr), allocated_voter_rewards(avr) {}

    EOSLIB_SERIALIZE(bp_info_t, (total_votes)(allocated_bp_rewards)(allocated_voter_rewards) )
};

typedef std::pair<name, bp_info_t> bp_entry_t;
/**
 * election round table, one record per day
 *
 * for current onging round,
 */
struct CONTRACT_TBL election_round_t{
    uint64_t round_id = 0;          //usually one day one round
    uint64_t next_round_id = 0;     //upon arrival of new round, record it here
    time_point started_at;
    time_point ended_at;
    time_point created_at;

    uint64_t vote_count                 = 0;
    uint64_t unvote_count               = 0;

    bool  vote_tally_completed          = false;
    bool  unvote_last_round_completed   = false;
    bool  reward_allocation_completed   = false;

    asset total_votes                   = asset(0, SYS_SYMBOL);
    asset total_voteage                 = asset(0, SYS_SYMBOL);
    asset total_rewarded                = asset(0, SYS_SYMBOL); //total received accumualted rewards

    std::map<name, bp_info_t> elected_bps;      //max 50 bps (but top 21 get rewarded only)

    election_round_t() {}
    election_round_t(uint64_t rid): round_id(rid) {}

    uint64_t primary_key()const { return round_id; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"electrounds"_n, election_round_t> tbl_t;

    EOSLIB_SERIALIZE(election_round_t,  (round_id)(next_round_id)(started_at)(ended_at)(created_at)
                                        (vote_count)(unvote_count)
                                        (vote_tally_completed)(unvote_last_round_completed)(reward_allocation_completed)
                                        (total_votes)(total_voteage)(total_rewarded)
                                        (elected_bps) )
};

// added by BP candidate
struct CONTRACT_TBL candidate_t {
    name owner;
    uint32_t self_reward_share;     //boost by 10000
    asset staked_votes;             //self staked
    asset received_votes;           //other voted
    asset tallied_votes;            //for election sorting usage, updated upon vote/unvote tally
    asset last_claimed_rewards;     //unclaimed total rewards
    asset total_claimed_rewards;    //unclaimed total rewards
    asset unclaimed_rewards;        //unclaimed total rewards

    candidate_t() {}
    candidate_t(const name& o): owner(o) {}

    uint64_t primary_key() const { return owner.value; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"candidates"_n, candidate_t> tbl_t;

    EOSLIB_SERIALIZE(candidate_t,   (owner)(self_reward_share)
                                    (staked_votes)(received_votes)(tallied_votes)
                                    (last_claimed_rewards)(total_claimed_rewards)(unclaimed_rewards) )
};

struct CONTRACT_TBL voter_t {
    name                owner;                  //the voter
    asset               total_staked;           //sum of all votes
    asset               last_claimed_rewards;   //unclaimed total rewards
    asset               total_claimed_rewards;  //unclaimed total rewards
    asset               unclaimed_rewards;      //unclaimed total rewards

    voter_t() {}
    voter_t(const name& o): owner(o) {}

    uint64_t primary_key() const { return owner.value; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"voters"_n, voter_t> tbl_t;

    EOSLIB_SERIALIZE( voter_t,  (owner)(total_staked)
                                (last_claimed_rewards)(total_claimed_rewards)(unclaimed_rewards) )
};

/**
 *  vote table
 */
struct CONTRACT_TBL vote_t {
    uint64_t id;        //PK: available_primary_key

    name owner;         //voter
    name candidate;     //target candidiate to vote for
    asset quantity;

    time_point voted_at;
    time_point unvoted_at;
    time_point restarted_at;   //restart age counting every 30-days
    uint64_t election_round;
    uint64_t reward_round;

    uint64_t by_voter() const             { return owner.value;                              }
    uint64_t by_candidate() const         { return candidate.value;                          }
    uint64_t by_voted_at() const          { return uint64_t(voted_at.sec_since_epoch());     }
    uint64_t by_unvoted_at() const        { return uint64_t(unvoted_at.sec_since_epoch());   }
    uint64_t by_restarted_at() const      { return uint64_t(restarted_at.sec_since_epoch()); }
    uint64_t by_election_round() const    { return election_round;                           }
    uint64_t by_reward_round() const      { return reward_round;                             }

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }
    
    typedef eosio::multi_index
    < "votes"_n, vote_t,
        indexed_by<"voter"_n,        const_mem_fun<vote_t, uint64_t, &vote_t::by_voter>             >,
        indexed_by<"candidate"_n,    const_mem_fun<vote_t, uint64_t, &vote_t::by_candidate>         >,
        indexed_by<"voteda"_n,       const_mem_fun<vote_t, uint64_t, &vote_t::by_voted_at>          >,
        indexed_by<"unvoteda"_n,     const_mem_fun<vote_t, uint64_t, &vote_t::by_unvoted_at>        >,
        indexed_by<"restarted"_n,    const_mem_fun<vote_t, uint64_t, &vote_t::by_restarted_at>      >,
        indexed_by<"electround"_n,   const_mem_fun<vote_t, uint64_t, &vote_t::by_election_round>    >,
        indexed_by<"rewardround"_n,  const_mem_fun<vote_t, uint64_t, &vote_t::by_reward_round>      >
    > tbl_t;
    
    vote_t() {}
    vote_t(const uint64_t& pk): id(pk) {}

    EOSLIB_SERIALIZE( vote_t,   (id)(owner)(candidate)(quantity)
                                (voted_at)(unvoted_at)(restarted_at)
                                (election_round)(reward_round) )
};

/**
 *  Incoming rewards for whole bpvoting cohort
 *
 */
struct CONTRACT_TBL reward_t {
    uint64_t id;
    asset quantity;
    time_point created_at;

    reward_t() {}
    reward_t(name code, uint64_t scope) {
        tbl_t tbl(code, scope);
        id = tbl.available_primary_key();
    }

    reward_t(const uint64_t& i): id(i) {}

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"rewards"_n, reward_t> tbl_t;

    EOSLIB_SERIALIZE( reward_t,  (id)(quantity)(created_at) )
};

struct CONTRACT_TBL unvote_t {
    uint64_t id;
    name owner;
    asset quantity;
    time_point_sec created_at;
    time_point_sec refundable_at;
    
    unvote_t() {}
    unvote_t(const uint64_t& i): id(i) {}

    uint64_t primary_key() const { return id; }
    uint64_t scope()const { return 0; }

    uint64_t by_owner()const         { return owner.value;                           }
    uint64_t by_created_at()const    { return uint64_t(created_at.sec_since_epoch()); } 
    uint64_t by_refundable_at()const { return uint64_t(refundable_at.sec_since_epoch()); }
    typedef eosio::multi_index
    <"unvotes"_n, unvote_t ,
        indexed_by<"owner"_n,     const_mem_fun<unvote_t, uint64_t, &unvote_t::by_owner> >,
        indexed_by<"unvote"_n,    const_mem_fun<unvote_t, uint64_t, &unvote_t::by_refundable_at> >
    > tbl_t;

    EOSLIB_SERIALIZE(unvote_t,  (id)(owner)(quantity)(created_at)(refundable_at) )
};

}