#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include "wasm_db.hpp"
#include "otcstore_entities.hpp"

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

class [[eosio::contract("mgp.otcstore")]] mgp_otcstore: public eosio::contract {
  private:
    global_singleton    _global;
    global_t            _gstate;
    dbc                 _dbc;

  public:
    using contract::contract;
    mgp_otcstore(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value), _dbc(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~mgp_otcstore() {
        _global.set( _gstate, get_self() );
    }

    [[eosio::action]] //only code maintainer can init
    void init();

    [[eosio::action]]
    void setseller(const name& owner, const set<PaymentType>pay_methods, const string& memo_to_buyer);
    
    /**
     * seller to open sell order
     */
    [[eosio::action]]
    void openorder(const name& owner, const asset& quantity, const asset& price, const asset& min_accept_quantity);

    [[eosio::action]]
    void closeorder(const name& owner, const uint64_t& order_id);

    [[eosio::action]]
    void opendeal(const name& taker, const uint64_t& order_id, const asset& deal_quantity);

    [[eosio::action]]
    void closedeal(const name& taker, const uint64_t& deal_id);
    
    /**
     *  @param: user_type -> 0: buyer, 1: seller, 2: otc-arbiter
     *  @param: pass: 0: NO pass, 1: pass; two agreed passes means a decision! 
     */
    [[eosio::action]]
    void passdeal(const name& owner, const UserType& user_type, const uint64_t& deal_id, const bool& pass);

    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(name from, name to, asset quantity, string memo);

    using init_action       = action_wrapper<name("init"),        &mgp_otcstore::init       >;
    using setseller_action  = action_wrapper<name("setseller"),   &mgp_otcstore::setseller  >;
    using orderorder_action = action_wrapper<name("openorder"),   &mgp_otcstore::openorder  >;
    using closeorder_action = action_wrapper<name("closeorder"),  &mgp_otcstore::closeorder >;
    
    using orderdeal_action = action_wrapper<name("opendeal"),     &mgp_otcstore::opendeal   >;
    using closedeal_action = action_wrapper<name("closedeal"),    &mgp_otcstore::closedeal  >;
    using passdeal_action  = action_wrapper<name("passdeal"),     &mgp_otcstore::passdeal   >;

    using transfer_action = action_wrapper<name("transfer"),      &mgp_otcstore::deposit    >;

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