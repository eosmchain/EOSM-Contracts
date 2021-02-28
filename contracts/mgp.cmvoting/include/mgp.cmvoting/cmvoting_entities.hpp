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

struct [[eosio::table("global"), eosio::contract("mgp.cmvoting")]] global_t {
    
    bool scheme;
    bool vote;
    bool award;
    asset deposit;
    asset vote_count;
    set<name> auditors;
    time_point_sec closed_at;
    
    global_t() {
        scheme = false;
        vote = false;
        award = false;
        deposit = asset(1, SYS_SYMBOL);
        vote_count = asset(0, SYS_SYMBOL);
    }

    EOSLIB_SERIALIZE( global_t, (scheme)(vote)
                                (award)(deposit)(vote_count)(auditors)(closed_at))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


enum proposal_audit_status: uint8_t {
    STARTED       = 0,
    APPROVED      = 1,
    DISAPPROVED   = 2
};
/**
 * 创建投递方案表
 **/
struct CONTRACT_TBL proposal_t{
    uint64_t id;        //id
    name account;       //name账户类型
    string title;       //方案主题
    string content;     //方案内容
    asset deposit;      //押金
    asset vote_count;   //票数
    bool refunded;      //押金是否已退
    proposal_audit_status audit_status;
    string audit_result;
    time_point_sec created_at; //创建时间
    time_point_sec updated_at; //更新时间

    uint64_t primary_key() const { return id; }
    proposal_t(){}
    proposal_t(const uint64_t& i): id(i) {}
    
    uint64_t by_account() const {return account.value;}
    uint64_t by_votecount() const {return std::numeric_limits<uint64_t>::max() - vote_count.amount;}
    uint64_t by_auditstatus() const {return audit_status;}
    
    EOSLIB_SERIALIZE( proposal_t, (id)(account)(title)(content)(deposit)(vote_count)(refunded)
                                  (audit_status)(audit_result)
                                  (created_at)(updated_at) )

};
    
//typedef
typedef eosio::multi_index
    < "schemes"_n, proposal_t,
        indexed_by<"account"_n, const_mem_fun<proposal_t, uint64_t, &proposal_t::by_account> >,
        indexed_by<"votecount"_n, const_mem_fun<proposal_t, uint64_t, &proposal_t::by_votecount> >,
        indexed_by<"auditstatus"_n, const_mem_fun<proposal_t, uint64_t, &proposal_t::by_auditstatus> >
    > scheme_tbl;//表

/**
 * 投票记录
 *
 **/
 struct CONTRACT_TBL vote_t {
    uint64_t id; //id
    name account;  //投票人
    asset vote_count; //投递的票数
    uint64_t proposal_id; //方案人ID
    bool refunded; //是否已打款 true 已打款 false未打款
    bool is_super_node; //是否是超级节点发起的投票
    string title; //方案主题
    string abstract; //方案内容摘要
    time_point_sec created_at; //创建时间

    uint64_t primary_key() const { return id; }
    vote_t(){}
    vote_t(const uint64_t& i): id(i) {}
    uint64_t by_account() const {return account.value;}
    
    EOSLIB_SERIALIZE(vote_t, (id)(account)(vote_count)(proposal_id)
                             (refunded)(is_super_node)(title)(abstract)
                             (created_at) )
 
 };
//typedef
typedef eosio::multi_index
    < "votedetails"_n, vote_t,
        indexed_by<"account"_n, const_mem_fun<vote_t, uint64_t, &vote_t::by_account> >
    > vote_tbl;//表

struct CONTRACT_TBL vote_record{
    uint64_t id;
    uint64_t proposal_id; //方案人ID
    name account;
    asset vote_count;
    string title; //方案主题
    string content; //方案内容
    time_point_sec created_at;

    uint64_t primary_key() const { return id; }
    vote_record(){}
    vote_record(const uint64_t& i): id(i) {}
    uint64_t by_account() const {return account.value;}

    EOSLIB_SERIALIZE( vote_record, (id)(account)(vote_count)(title)(content)(proposal_id)
                                    (created_at) )
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
    uint64_t proposal_id; //方案人通过ID
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
    EOSLIB_SERIALIZE(settle_t,(id)(proposal_id)(total_vote)(super_node_vote)(ordinary_vote_count)(super_node_suc)(total_super_node)(scheme_award)(vote_award)(created_at))


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