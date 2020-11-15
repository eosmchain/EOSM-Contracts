#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include "wasm_db.hpp"
#include "bpvoting_entities.hpp"

using namespace wasm::db;

namespace mgp {

using eosio::asset;
using eosio::check;
using eosio::datastream;
using eosio::name;
using eosio::symbol;
using eosio::symbol_code;
using eosio::unsigned_int;

using std::string;

static constexpr eosio::name active_perm{"active"_n};
static constexpr eosio::name token_account{"eosio.token"_n};
static constexpr symbol SYS_SYMBOL = symbol(symbol_code("MGP"), 4);
static constexpr uint32_t seconds_per_year      = 24 * 3600 * 7 * 52;
static constexpr uint32_t seconds_per_month     = 24 * 3600 * 30;
static constexpr uint32_t seconds_per_week      = 24 * 3600 * 7;
static constexpr uint32_t seconds_per_day       = 24 * 3600;
static constexpr uint32_t seconds_per_hour      = 3600;
static constexpr uint32_t rewards_to_bp_per_day = 1580;
static constexpr uint32_t min_votes             = 100;

class [[eosio::contract("mgp.bpvoting")]] mgp_bpvoting: public eosio::contract {
  private:
    global_singleton    _global;
    global_t            _gstate;
    dbc                 _dbc;

  public:
    using contract::contract;
    mgp_bpvoting(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value), _dbc(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~mgp_bpvoting() {
        _global.set( _gstate, get_self() );
    }

    [[eosio::action]]
    void chvote(const name& owner, const name& from_candidate, const name& to_candidate, const asset& quantity);
    [[eosio::action]]
    void unvote(const name& owner, const uint64_t vote_id, const asset& quantity);
    [[eosio::action]]
    void execute(const name& issuer); //anyone can invoke, but usually by the platform

  public:
    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(name from, name to, asset quantity, string memo);
  
    using chvote_action   = action_wrapper<name("chvote"),    &mgp_bpvoting::chvote>;
    using unvote_action   = action_wrapper<name("unvote"),    &mgp_bpvoting::unvote>;
    using execute_action  = action_wrapper<name("execute"),   &mgp_bpvoting::execute>;
    using transfer_action = action_wrapper<name("transfer"),  &mgp_bpvoting::deposit>;

  private:
    void _list(const name& owner, const asset& quantity, const uint8_t& voter_reward_share_percent);
    void _vote(const name& owner, const name& target, const asset& quantity);
    void _elect(map<name, asset>& elected_bps, const candidate_t& candidate);

    uint64_t get_round_id(const time_point& ct);
    void _current_election_round(const time_point& ct, election_round_t& election_round);
    void _tally_votes_for_election_round(election_round_t& round);
    void _tally_unvotes_for_election_round(election_round_t& round);
    void _reward_through_votes(election_round_t& round);
};

// EOSIO_DISPATCH( mgp_bpvoting, (chvote)(unvote)(execute) )

inline vector <string> string_split(string str, char delimiter) {
      vector <string> r;
      string tmpstr;
      while (!str.empty()) {
          int ind = str.find_first_of(delimiter);
          if (ind == -1) {
              r.push_back(str);
              str.clear();
          } else {
              r.push_back(str.substr(0, ind));
              str = str.substr(ind + 1, str.size() - ind - 1);
          }
      }
      return r;

  }

}