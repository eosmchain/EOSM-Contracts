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
static constexpr eosio::name token_account{"eosio.token"_n};

static constexpr symbol SYS_SYMBOL              = symbol(symbol_code("MGP"), 4);
static constexpr uint32_t seconds_per_year      = 24 * 3600 * 7 * 52;
static constexpr uint32_t seconds_per_month     = 24 * 3600 * 30;
static constexpr uint32_t seconds_per_week      = 24 * 3600 * 7;
static constexpr uint32_t seconds_per_day       = 24 * 3600;
static constexpr uint32_t seconds_per_hour      = 3600;
static constexpr uint32_t rewards_to_bp_per_day = 1580;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.bpvoting")]]

struct [[eosio::table("global"), eosio::contract("mgp.bpvoting")]] global_t {
    uint64_t max_iterate_steps_tally_vote;
    uint64_t max_iterate_steps_tally_unvote;
    uint64_t max_iterate_steps_reward;
    uint64_t max_bp_size;
    uint64_t max_candidate_size;
    uint64_t bp_rewards_per_day;        //for one BP
    uint64_t refund_time;
    uint64_t election_round_begin_at;   //GMT+0 Time
    asset min_bp_list_quantity;
    asset min_bp_accept_quantity;
    asset min_bp_vote_quantity;
    asset total_listed;
    asset total_staked;
    asset total_rewarded;
    time_point started_at;

    global_t() {
        max_iterate_steps_tally_vote    = 30;
        max_iterate_steps_tally_unvote  = 20;
        max_iterate_steps_reward        = 50;
        max_bp_size                     = 21;
        max_candidate_size              = 30;
        bp_rewards_per_day              = 1580;
        refund_time                     = 3 * 24 * 3600; //3-days in sec
        election_round_begin_at         = 0; //i.e. GMT+8 Shanghai Time, 24hrs per round
        min_bp_list_quantity            = asset(100'000'0000ll, SYS_SYMBOL);
        min_bp_accept_quantity          = asset(200'000'0000ll, SYS_SYMBOL);
        min_bp_vote_quantity            = asset(10'0000ll, SYS_SYMBOL); //10 MGP at least!
        total_listed                    = asset(0, SYS_SYMBOL);
        total_staked                    = asset(0, SYS_SYMBOL);
        total_rewarded                  = asset(0, SYS_SYMBOL);
    }

    EOSLIB_SERIALIZE( global_t, (max_iterate_steps_tally_vote)(max_iterate_steps_tally_unvote)
                                (max_iterate_steps_reward)(max_bp_size)(max_candidate_size)
                                (bp_rewards_per_day)(refund_time)(election_round_begin_at)
                                (min_bp_list_quantity)(min_bp_accept_quantity)(min_bp_vote_quantity)
                                (total_listed)(total_staked)(total_rewarded)
                                (started_at) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

/**
 * election round table, one record per day
 *
 * for current onging round,
 */
struct CONTRACT_TBL election_round_t{
    uint64_t round_id;          //one day one round
    time_point started_at;
    time_point ended_at;

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
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"electrounds"_n, election_round_t> index_t;

    EOSLIB_SERIALIZE(election_round_t,  (round_id)(started_at)(ended_at)
                                        (vote_count)(unvote_count)(vote_tallied_count)(unvote_tallied_count)
                                        (vote_tally_completed)(unvote_tally_completed)(reward_completed)(execute_completed)
                                        (elected_bps)(total_votes)(total_votes_in_coinage)
                                        (available_rewards)(total_rewards) )
};

// added by BP candidate
struct CONTRACT_TBL candidate_t {
    name owner;
    uint32_t self_reward_share;     //boost by 10000
    asset staked_votes;             //self staked
    asset received_votes;           //other voted
    asset last_claimed_rewards;     //unclaimed total rewards
    asset total_claimed_rewards;    //unclaimed total rewards
    asset unclaimed_rewards;        //unclaimed total rewards

    candidate_t() {}
    candidate_t(const name& o): owner(o) {}

    uint64_t primary_key() const { return owner.value; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"candidates"_n, candidate_t> index_t;

    EOSLIB_SERIALIZE(candidate_t,   (owner)(self_reward_share)
                                    (staked_votes)(received_votes)
                                    (last_claimed_rewards)(total_claimed_rewards)(unclaimed_rewards) )
};

/**
 *  vote table
 */
struct CONTRACT_TBL vote_t {
    uint64_t id;        //available_primary_key
    name owner;         //voter
    name candidate;     //target candidiate to vote for
    asset quantity;

    time_point voted_at;
    time_point unvoted_at;
    time_point restarted_at;   //restart age counting every 30-days
    time_point last_vote_tallied_at;
    time_point last_unvote_tallied_at;
    time_point last_rewarded_at;

    uint64_t by_voted_at() const                { return uint64_t(voted_at.sec_since_epoch());                }
    uint64_t by_unvoted_at() const              { return uint64_t(unvoted_at.sec_since_epoch());              }
    uint64_t by_restarted_at() const            { return uint64_t(restarted_at.sec_since_epoch());            }
    uint64_t by_last_vote_tallied_at() const    { return uint64_t(last_vote_tallied_at.sec_since_epoch());    }
    uint64_t by_last_unvote_tallied_at() const  { return uint64_t(last_unvote_tallied_at.sec_since_epoch());  }
    uint64_t by_last_rewarded_at() const        { return uint64_t(last_rewarded_at.sec_since_epoch());        }

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }

    vote_t() {}
    vote_t(name code, uint64_t scope) {
        index_t tbl(code, scope);
        id = tbl.available_primary_key();
    }
    vote_t(uint64_t i): id(i) {}

    typedef eosio::multi_index<"votes"_n, vote_t> index_t;

    EOSLIB_SERIALIZE( vote_t,   (id)(owner)(candidate)(quantity)
                                (voted_at)(unvoted_at)(restarted_at)
                                (last_vote_tallied_at)(last_unvote_tallied_at)(last_rewarded_at) )
};

typedef eosio::multi_index < "votes"_n, vote_t,
    indexed_by<"voteda"_n,          const_mem_fun<vote_t, uint64_t, &vote_t::by_voted_at> >,
    indexed_by<"unvoteda"_n,        const_mem_fun<vote_t, uint64_t, &vote_t::by_unvoted_at> >,
    indexed_by<"restarted"_n,       const_mem_fun<vote_t, uint64_t, &vote_t::by_restarted_at> >,
    indexed_by<"lvotallied"_n,      const_mem_fun<vote_t, uint64_t, &vote_t::by_last_vote_tallied_at> >,
    indexed_by<"luvtallied"_n,      const_mem_fun<vote_t, uint64_t, &vote_t::by_last_unvote_tallied_at> >,
    indexed_by<"lastrewarded"_n,    const_mem_fun<vote_t, uint64_t, &vote_t::by_last_rewarded_at> >
    > vote_multi_index_tbl;

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

    typedef eosio::multi_index<"voters"_n, voter_t> index_t;

    EOSLIB_SERIALIZE( voter_t,  (owner)(total_staked)
                                (last_claimed_rewards)(total_claimed_rewards)(unclaimed_rewards) )
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
        index_t tbl(code, scope);
        id = tbl.available_primary_key();
    }

    reward_t(const uint64_t& i): id(i) {}

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"rewards"_n, reward_t> index_t;

    EOSLIB_SERIALIZE( reward_t,  (id)(quantity)(created_at) )
};

}