#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include "wasm_db.hpp"
#include "entities.hpp"

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

static constexpr bool DEBUG = true;

#define WASM_FUNCTION_PRINT_LENGTH 50

#define MGP_LOG( debug, exception, ... ) {  \
if ( debug ) {                               \
   std::string str = std::string(__FILE__); \
   str += std::string(":");                 \
   str += std::to_string(__LINE__);         \
   str += std::string(":[");                \
   str += std::string(__FUNCTION__);        \
   str += std::string("]");                 \
   while(str.size() <= WASM_FUNCTION_PRINT_LENGTH) str += std::string(" ");\
   eosio::print(str);                                                             \
   eosio::print( __VA_ARGS__ ); }}

class [[eosio::contract("mgp.bpvoting")]] mgp_bpvoting: public eosio::contract {
  private:
    global_singleton    _global;
    global_t            _gstate;
    global2_singleton   _global2;
    global2_t           _gstate2;
    dbc                 _dbc;

  public:
    using contract::contract;
    mgp_bpvoting(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value), 
        _global2(get_self(), get_self().value), _dbc(get_self())
    {
      _gstate = _global.exists() ? _global.get() : global_t{};
      _gstate2 = _global2.exists() ? _global2.get() : global2_t{};
    }

    ~mgp_bpvoting() {
      _global.set( _gstate, get_self() );
      _global2.set( _gstate2, get_self() );
    }

    [[eosio::action]]
    void init();  //only code maintainer can init

    [[eosio::action]]
    void unvote(const name& owner, const uint64_t vote_id);

    [[eosio::action]]
    void unvotex(const uint64_t vote_id);

    [[eosio::action]]
    void unvoteuser(const name& user, const asset& quant);
    
    [[eosio::action]]
    void unstakeuser(const name& user, const asset& quant);

    [[eosio::action]]
    void execute(); //anyone can invoke, but usually by the platform

    [[eosio::action]]
    void delist(const name& issuer); //candidate to delist self

    [[eosio::action]]
    void claimrewards(const name& issuer, const bool is_voter); //voter/candidate to claim rewards

    [[eosio::action]]
    void refreshtally(const name& candidate);

    [[eosio::action]]
    void checkvotes(const name& voter, const uint64_t& last_election_round);

    [[eosio::on_notify("eosio.token::transfer")]]
    void ontransfer(name from, name to, asset quantity, string memo);

    [[eosio::action]]
    void refunds();

  private:
    uint64_t get_round_id(const time_point& ct);
    
    void _list(const name& owner, const asset& quantity, const uint32_t& voter_reward_share_percent);
    void _vote(const name& owner, const name& target, const asset& quantity);
    void _elect(election_round_t& round, const candidate_t& candidate);
    void _current_election_round(const time_point& ct, election_round_t& election_round);
    void _tally_votes(election_round_t& last_round, election_round_t& execution_round);
    void _apply_unvotes(election_round_t& round);
    void _allocate_rewards(election_round_t& round);
    void _execute_rewards(election_round_t& round);

    /** Init functions **/
    void _init();
    void _referesh_recvd_votes();
    void _referesh_tallied_votes(const name& candidate);
    void _referesh_ers(uint64_t round);

};

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