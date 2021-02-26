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
static constexpr eosio::name cs_contact{""_n};
static constexpr symbol   SYS_SYMBOL            = symbol(symbol_code("MGP"), 4);



#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.cmvoting")]]

struct [[eosio::table("configs"), eosio::contract("mgp.cmvoting")]] global_t {
    
    bool scheme;
    bool vote;
    bool award;
    asset cash_money;
    asset vote_count;
    string close_time;

    global_t() {
        scheme = false;
        vote = false;
        award = false;
        cash_money = asset(1.0000,SYS_SYMBOL);
        vote_count = asset(0.0000,SYS_SYMBOL);
        close_time = "00-00-00";
    }

    EOSLIB_SERIALIZE( global_t, (scheme)(vote)
                                (award)(cash_money)(vote_count)(close_time))
};
typedef eosio::singleton< "configs"_n, global_t > global_singleton;



/**
 * 创建投递方案表
 **/
struct CONTRACT_TBL scheme_t{

    name account; //name账户类型
    uint64_t id; //id
    string scheme_title; //方案主题
    string scheme_content; //方案内容
    time_point_sec created_at; //创建时间
    time_point_sec updated_at; //更新时间
    asset vote_count; //票数
    bool is_del; //状态0正常1删除状态
    asset cash_money;//押金
    bool is_remit; //是否打款
    uint64_t audit_status;
    string audit_msg;
    uint64_t primary_key() const { return id; }
    scheme_t(){}
    scheme_t(const uint64_t& i): id(i) {}
    uint64_t by_account() const {return account.value;}
    uint64_t by_votecount() const {return std::numeric_limits<uint64_t>::max() - vote_count.amount;}
    uint64_t by_auditstatus() const {return audit_status;}
    EOSLIB_SERIALIZE(scheme_t,(account)(id)(scheme_title)(scheme_content)(created_at)(updated_at)(vote_count)(is_del)(cash_money)(is_remit)(audit_status)(audit_msg))

};
    
//typedef
typedef eosio::multi_index
    < "schemes"_n, scheme_t,
        indexed_by<"account"_n, const_mem_fun<scheme_t, uint64_t, &scheme_t::by_account> >,
        indexed_by<"votecount"_n, const_mem_fun<scheme_t, uint64_t, &scheme_t::by_votecount> >,
        indexed_by<"auditstatus"_n, const_mem_fun<scheme_t, uint64_t, &scheme_t::by_auditstatus> >
    > scheme_tbl;//表

/**
 * 投票记录
 *
 **/
 struct CONTRACT_TBL vote_t{
   
    uint64_t id; //id
    name account;  //投票人
    time_point_sec created_at; //创建时间
    asset vote_count; //投递的票数
    uint64_t scheme_id; //方案人ID
    bool is_remit; //是否已打款 true 已打款 false未打款
    bool is_super_node; //是否是超级节点发起的投票
    bool is_del; 
    string scheme_title; //方案主题
    string scheme_content; //方案内容
    uint64_t primary_key() const { return id; }
    vote_t(){}
    vote_t(const uint64_t& i): id(i) {}
    uint64_t by_account() const {return account.value;}
    EOSLIB_SERIALIZE(vote_t,(id)(account)(created_at)(vote_count)(scheme_id)(is_remit)(is_super_node)(is_del)(scheme_title)(scheme_content))
 
 };
//typedef
typedef eosio::multi_index
    < "votedetails"_n, vote_t,
        indexed_by<"account"_n, const_mem_fun<vote_t, uint64_t, &vote_t::by_account> >
    > vote_tbl;//表

struct CONTRACT_TBL vote_record{

    uint64_t id;
    name account;
    asset vote_count;
    time_point_sec created_at;
    string scheme_title; //方案主题
    string scheme_content; //方案内容
    uint64_t scheme_id; //方案人ID
    uint64_t primary_key() const { return id; }
    vote_record(){}
    vote_record(const uint64_t& i): id(i) {}
    uint64_t by_account() const {return account.value;}
    EOSLIB_SERIALIZE(vote_record,(id)(account)(vote_count)(created_at)(scheme_title)(scheme_content)(scheme_id))
};

typedef eosio::multi_index
    < "voterecords"_n, vote_record,
        indexed_by<"account"_n, const_mem_fun<vote_record, uint64_t, &vote_record::by_account> >
    > vote_record_tbl;//表

/**
 * 奖励记录
 **/
 struct CONTRACT_TBL award_t{
 
    uint64_t id; //id
    name account;  //奖励人
    time_point_sec created_at; //创建时间
    asset money; //奖励金额
    uint64_t award_type; // 0方案押金退回1投票退回2方案奖励3投票奖励
    bool is_del; 
    award_t(){}
    award_t(const uint64_t& i): id(i) {}
    uint64_t by_account() const {return account.value;}
    uint64_t primary_key() const { return id; }
    EOSLIB_SERIALIZE(award_t,(id)(account)(created_at)(money)(award_type)(is_del))
 };
 //typedef
typedef eosio::multi_index
    < "awards"_n, award_t,
        indexed_by<"account"_n, const_mem_fun<award_t, uint64_t, &award_t::by_account> >
    > award_tbl;//表
//结算配置
struct CONTRACT_TBL settle_t{

    uint64_t id; //id
    uint64_t scheme_id; //方案人通过ID
    asset total_vote;//总票数
    asset super_node_vote;//超级节点投票数
    asset ordinary_vote_count;//普通投票
    bool super_node_suc;//普通投票总数是否超过超级节点的51%
    asset total_super_node;//超级节点总票数
    asset scheme_award;//方案奖励
    asset vote_award;//投票奖励
    time_point_sec created_at;
    settle_t(){}
    uint64_t primary_key() const { return id; }
    EOSLIB_SERIALIZE(settle_t,(id)(scheme_id)(total_vote)(super_node_vote)(ordinary_vote_count)(super_node_suc)(total_super_node)(scheme_award)(vote_award)(created_at))


};
 //typedef
typedef eosio::multi_index
    < "settles"_n, settle_t
    > settle_tbl;//表

struct CONTRACT_TBL seller_t {
    name owner;
    asset available_quantity = asset(0, SYS_SYMBOL);
    set<uint8_t> accepted_payments; //accepted payments
    uint32_t processed_deals = 0;
    string email;
    string memo;

    seller_t() {}
    seller_t(const name& o): owner(o) {}

    uint64_t primary_key()const { return owner.value; }
    uint64_t scope()const { return 0; }

    typedef eosio::multi_index<"sellers"_n, seller_t> tbl_t;

    EOSLIB_SERIALIZE(seller_t,  (owner)(available_quantity)(accepted_payments)
                                (processed_deals)(email)(memo) )
};

}