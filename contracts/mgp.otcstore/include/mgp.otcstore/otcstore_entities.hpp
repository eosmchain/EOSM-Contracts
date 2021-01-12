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
// static constexpr symbol   USD_SYMBOL            = symbol(symbol_code("USD"), 2);
static constexpr uint32_t seconds_per_year      = 24 * 3600 * 7 * 52;
static constexpr uint32_t seconds_per_month     = 24 * 3600 * 30;
static constexpr uint32_t seconds_per_week      = 24 * 3600 * 7;
static constexpr uint32_t seconds_per_day       = 24 * 3600;
static constexpr uint32_t seconds_per_hour      = 3600;
static constexpr uint32_t max_memo_size         = 1024;

#define CONTRACT_TBL [[eosio::table, eosio::contract("mgp.otcstore")]]

struct [[eosio::table("global"), eosio::contract("mgp.otcstore")]] global_t {
    asset min_buy_order_quantity;
    asset min_sell_order_quantity;
    asset min_pos_stake_quantity;
    name pos_staking_contract;
    uint64_t withhold_expire_sec;   // the amount hold will be unfrozen upon expiry
    name transaction_fee_receiver;  // receiver account to transaction fees
    uint64_t transaction_fee_ratio; // fee ratio boosted by 10000
    set<name> otc_arbiters;

    global_t() {
        min_buy_order_quantity      = asset(10, SYS_SYMBOL);
        min_sell_order_quantity     = asset(10, SYS_SYMBOL);
        min_pos_stake_quantity      = asset(0, SYS_SYMBOL);
        withhold_expire_sec         = 600; //10 mins
        transaction_fee_ratio       = 0;
    }

    EOSLIB_SERIALIZE( global_t, (min_buy_order_quantity)(min_sell_order_quantity)
                                (min_pos_stake_quantity)(pos_staking_contract)
                                (withhold_expire_sec)(transaction_fee_receiver)
                                (transaction_fee_ratio)(otc_arbiters) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

enum PaymentType: uint8_t {
    PAYMAX      = 0,
    BANK        = 1,
    WECAHAT     = 2,
    ALIPAY      = 3,
    MASTER      = 4,
    VISA        = 5,
    PAYPAL      = 6,


};

enum UserType: uint8_t {
    MAKER       = 0,    //seller
    TAKER       = 1,    //buyer
    ARBITER     = 2
};

/**
 * Generic order struct for buyers/sellers
 * when the owner decides to close it before complete fulfillment, it just get erased
 * if it is truly fufilled, it also get deleted.
 */
struct CONTRACT_TBL order_t {
    uint64_t id;                //PK: available_primary_key
    
    name owner;                 //order maker's account
    asset price;                // MGP price the buyer willing to buy, symbol CNY|USD
    asset quantity;
    asset min_accept_quantity;
    asset frozen_quantity;
    asset fulfilled_quantity;    //support partial fulfillment
    bool closed;
    time_point_sec created_at;
    time_point_sec closed_at;

    order_t() {}
    order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return price.symbol.code().raw(); }

    uint64_t by_price() const { return closed ? -1 : price.amount; } //for seller: smaller & higher
    uint64_t by_invprice() const { return closed ? 0 : -price.amount; }  //for buyer: bigger & higher
    uint64_t by_maker() const { return owner.value; } 

    EOSLIB_SERIALIZE(order_t,   (id)(owner)(price)(quantity)(min_accept_quantity)
                                (frozen_quantity)(fulfilled_quantity)
                                (closed)(created_at)(closed_at) )
};

// typedef eosio::multi_index
//     < "buyorders"_n,  order_t,
//         indexed_by<"price"_n, const_mem_fun<order_t, uint64_t, &order_t::by_invprice> >,
//         indexed_by<"maker"_n, const_mem_fun<order_t, uint64_t, &order_t::by_maker> >
//     > sk_buyorder_t;

typedef eosio::multi_index
    < "selorders"_n, order_t,
        indexed_by<"price"_n, const_mem_fun<order_t, uint64_t, &order_t::by_price> >,
        indexed_by<"maker"_n, const_mem_fun<order_t, uint64_t, &order_t::by_maker> >
    > sell_order_t;

/**
 * buy&sell deal
 *
 */
struct CONTRACT_TBL deal_t {
    uint64_t id;                //PK: available_primary_key

    uint64_t order_id;
    asset order_price;
    asset deal_quantity;

    name order_maker;
    bool maker_passed;
    time_point_sec maker_passed_at;

    name order_taker;
    bool taker_passed;
    time_point_sec taker_passed_at;

    name arbiter;
    bool arbiter_passed;
    time_point_sec arbiter_passed_at;

    bool closed;
    time_point_sec created_at;
    time_point_sec closed_at;

    uint64_t order_sn; // 订单号（前端生成）
    uint8_t pay_type; // 选择的支付类型
    time_point_sec expiration_at; // 订单到期时间

    deal_t() {}
    deal_t(uint64_t i): id(i) {}

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }
    // uint64_t scope() const { return order_price.symbol.code().raw(); }

    uint64_t by_order()     const { return order_id; }
    uint64_t by_maker()     const { return order_maker.value; }
    uint64_t by_taker()     const { return order_taker.value; }
    uint64_t by_arbiter()   const { return arbiter.value; }
    uint64_t by_ordersn()   const { return order_sn;}
    uint64_t by_expiration_at() const    { return uint64_t(expiration_at.sec_since_epoch()); }

    EOSLIB_SERIALIZE(deal_t,    (id)(order_id)(order_price)(deal_quantity)
                                (order_maker)(maker_passed)(maker_passed_at)
                                (order_taker)(taker_passed)(taker_passed_at)
                                (arbiter)(arbiter_passed)(arbiter_passed_at)
                                (closed)(created_at)(closed_at)(order_sn)(pay_type) 
                                (expiration_at))
};

typedef eosio::multi_index
    <"deals"_n, deal_t, 
        indexed_by<"order"_n,   const_mem_fun<deal_t, uint64_t, &deal_t::by_order> >,
        indexed_by<"maker"_n,   const_mem_fun<deal_t, uint64_t, &deal_t::by_maker> >,
        indexed_by<"taker"_n,   const_mem_fun<deal_t, uint64_t, &deal_t::by_taker> >,
        indexed_by<"arbiter"_n, const_mem_fun<deal_t, uint64_t, &deal_t::by_arbiter> >,
        indexed_by<"ordersn"_n, const_mem_fun<deal_t, uint64_t, &deal_t::by_ordersn> >,
        indexed_by<"expirationed"_n, const_mem_fun<deal_t, uint64_t, &deal_t::by_expiration_at> >
    > sk_deal_t;

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

/**
 * 交易订单过期时间
 *
 */
struct CONTRACT_TBL expiration_t{
    uint64_t deal_id;
    time_point_sec expiration_at;

    expiration_t() {}
    expiration_t(uint64_t i): deal_id(i) {}

    uint64_t primary_key()const { return deal_id; }
    uint64_t scope()const { return 0; }

    uint64_t by_expiration_at() const    { return uint64_t(expiration_at.sec_since_epoch()); }

    EOSLIB_SERIALIZE(expiration_t,  (deal_id)(expiration_at) )
};
typedef eosio::multi_index
    <"expirations"_n, expiration_t ,
        indexed_by<"expirationed"_n,    const_mem_fun<expiration_t, uint64_t, &expiration_t::by_expiration_at>   >
    > exp_tal_t;

} // MGP
