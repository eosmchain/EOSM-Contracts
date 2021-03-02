#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <string>

#include "wasm_db.hpp"
#include "cmvoting_entities.hpp"

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

class [[eosio::contract("mgp.cmvoting")]] mgp_cmvoting: public eosio::contract {
  private:
    global_singleton    _global;
    global_t            _gstate;
    dbc                 _dbc;

  public:
    using contract::contract;
    mgp_cmvoting(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), _global(get_self(), get_self().value), _dbc(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~mgp_cmvoting() {
        _global.set( _gstate, get_self() );
    }

    
    //scheme是否开启投递方案  vote是否开启投票  award是否开启结算
    [[eosio::action]]
    void control(const bool& scheme,const bool& vote,const bool& award,const asset& deposit,const asset& vote_count,const string& close_time);

    //提交方案 名称小写
    [[eosio::action]] 
    void addscheme(const name& account,const string& title,const string& content,const asset& deposit);

    //添加投票记录
    [[eosio::action]] 
    void addvote(const name& account,const asset& vote_count,const uint64_t& proposal_id,const bool& is_super_node,const string& title,const string& content);

    //退回方案押金 投票押金 结算
    [[eosio::action]]
    void award(const asset& super_node_count,const asset& scheme_award,const asset& vote_award);

    [[eosio::action]]
    void reset();

    //测试转账
    [[eosio::action]]
    void test(const name& toAddr,const asset& money);

    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(name from, name to, asset quantity, string memo);
    
    //审核方案
    [[eosio::action]]
    void audit(const uint64_t& id,const uint64_t& audit_status,const string& audit_result);

    //编辑方案
    [[eosio::action]]
    void upscheme(const uint64_t& proposal_id,const name& account,const string& title,const string& content);



    //注册

    using control_action = action_wrapper<name("control"),      &mgp_cmvoting::control    >;

    using addscheme_action = action_wrapper<name("addscheme"),      &mgp_cmvoting::addscheme    >;

    using addvote_action = action_wrapper<name("addvote"), &mgp_cmvoting::addvote   >;

    using award_action = action_wrapper<name("award"), &mgp_cmvoting::award   >;

    using del_action = action_wrapper<name("del"), &mgp_cmvoting::del   >;

    using test_action = action_wrapper<name("test"), &mgp_cmvoting::test   >;


    
};


}
