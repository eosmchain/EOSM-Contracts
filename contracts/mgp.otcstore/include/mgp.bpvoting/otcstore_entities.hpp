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

static constexpr symbol   SYS_SYMBOL            = symbol(symbol_code("MGP"), 4);
static constexpr symbol   CNY_SYMBOL            = symbol(symbol_code("CNY"), 2);
static constexpr symbol   USD_SYMBOL            = symbol(symbol_code("USD"), 2);
static constexpr uint32_t seconds_per_year      = 24 * 3600 * 7 * 52;
static constexpr uint32_t seconds_per_month     = 24 * 3600 * 30;
static constexpr uint32_t seconds_per_week      = 24 * 3600 * 7;
static constexpr uint32_t seconds_per_day       = 24 * 3600;
static constexpr uint32_t seconds_per_hour      = 3600;
static constexpr uint32_t share_boost           = 10000;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.otcstore")]]

struct [[eosio::table("global"), eosio::contract("mgp.otcstore")]] global_t {
    asset min_buy_order_quantity;
    asset min_sell_order_quantity;
    asset min_pos_stake_quantity;
    uint64_t withhold_expire_sec;   // upon taking order, the amount hold will be frozen until completion or cancellation
    name transaction_fee_receiver;  // receiver account to transaction fees
    uint64_t transaction_fee_ratio; // fee ratio boosted by 10000

    global_t() {
        min_buy_order_quantity      = asset(10, SYS_SYMBOL);
        min_sell_order_quantity     = asset(10, SYS_SYMBOL);
        min_pos_stake_quantity      = asset(0, SYS_SYMBOL);
        withhold_expire_sec         = 600; //10 mins
        transaction_fee_ratio       = 0;
    }

    EOSLIB_SERIALIZE( global_t, (min_buy_order_quantity)(min_sell_order_quantity)
                                (min_pos_stake_quantity)(withhold_expire_sec) 
                                (transaction_fee_receiver)(transaction_fee_ratio) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

/**
 * election round table, one record per day
 *
 * for current onging round,
 */
struct CONTRACT_TBL buy_order_t {
    uint64_t id;                //PK: available_primary_key
    
    name owner;                 //buyer's account
    string nickname;            //buyer's online nick
    string memo;                //buyer's memo to sellers

    asset quantity;
    asset min_accept_quantity;
    asset price;                // MGP price the buyer willing to buy, symbol CNY|USD
    uint8_t payment_type;       // 0: bank_transfer; 1: alipay; 2: wechat pay; 3: paypal; 4: master/visa
    time_point created_at;

    buy_order_t() {}
    buy_order_t(uint64_t i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"buyorders"_n, buy_order_t> index_t;

    EOSLIB_SERIALIZE(buy_order_t,  (round_id)(next_round_id)(started_at)(ended_at)(created_at)
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
    asset tallied_votes;            //received only, updated upon vote/unvote tally
    asset last_claimed_rewards;     //unclaimed total rewards
    asset total_claimed_rewards;    //unclaimed total rewards
    asset unclaimed_rewards;        //unclaimed total rewards

    candidate_t() {}
    candidate_t(const name& o): owner(o) {}

    uint64_t primary_key() const { return owner.value; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"candidates"_n, candidate_t> index_t;

    EOSLIB_SERIALIZE(candidate_t,   (owner)(self_reward_share)
                                    (staked_votes)(received_votes)(tallied_votes)
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
    typedef eosio::multi_index<"votes"_n, vote_t> index_t;
    
    vote_t() {}
    // vote_t(const name& code) {
    //     index_t tbl(code, code.value); //scope: o
    //     id = tbl.available_primary_key();
    // }
    vote_t(const uint64_t& pk): id(pk) {}

    EOSLIB_SERIALIZE( vote_t,   (id)(owner)(candidate)(quantity)
                                (voted_at)(unvoted_at)(restarted_at)
                                (election_round)(reward_round) )
};

typedef eosio::multi_index
< "votes"_n, vote_t,
    indexed_by<"voter"_n,        const_mem_fun<vote_t, uint64_t, &vote_t::by_voter>             >,
    indexed_by<"candidate"_n,    const_mem_fun<vote_t, uint64_t, &vote_t::by_candidate>         >,
    indexed_by<"voteda"_n,       const_mem_fun<vote_t, uint64_t, &vote_t::by_voted_at>          >,
    indexed_by<"unvoteda"_n,     const_mem_fun<vote_t, uint64_t, &vote_t::by_unvoted_at>        >,
    indexed_by<"restarted"_n,    const_mem_fun<vote_t, uint64_t, &vote_t::by_restarted_at>      >,
    indexed_by<"electround"_n,   const_mem_fun<vote_t, uint64_t, &vote_t::by_election_round>    >,
    indexed_by<"rewardround"_n,  const_mem_fun<vote_t, uint64_t, &vote_t::by_reward_round>      >
> vote_tbl;


/**
 *  vote table
 */
struct CONTRACT_TBL voteage_t {
    uint64_t vote_id;        //PK
    asset votes;
    uint64_t age;           //days

    voteage_t() {}
    voteage_t(const uint64_t vid): vote_id(vid) {}
    voteage_t(const uint64_t vid, const asset& v, const uint64_t a): vote_id(vid), votes(v), age(a) {}

    asset value() { return votes * age; }

    uint64_t primary_key() const { return vote_id; }
    uint64_t scope() const { return 0; }

    typedef eosio::multi_index<"voteages"_n, voteage_t> index_t;

    EOSLIB_SERIALIZE( voteage_t,  (vote_id)(votes)(age) )
};

/**
 *  Incoming rewards for whole otcstore cohort
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