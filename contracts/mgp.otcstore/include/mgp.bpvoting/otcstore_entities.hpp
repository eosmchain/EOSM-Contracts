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
static constexpr uint32_t max_nick_size         = 256;
static constexpr uint32_t max_memo_size         = 1024;

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
 * Buy order with CNY price
 *
 */
struct CONTRACT_TBL cny_buyorder_t {
    uint64_t id;                //PK: available_primary_key
    
    name owner;                 //account
    string nickname;            //online nick
    string memo;                //memo to buyer/sellers

    asset quantity;
    asset min_accept_quantity;
    asset price;                // MGP price the buyer willing to buy, symbol CNY|USD
    uint8_t payment_type;       // 0: bank_transfer; 1: alipay; 2: wechat pay; 3: paypal; 4: master/visa
    time_point created_at;

    cny_buyorder_t() {}
    cny_buyorder_t(uint64_t i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope() const { return 0; }

    uint64_t by_price() const { return price.amount; }
     
    typedef eosio::multi_index< "cnybuyorders"_n, cny_buyorder_t> pk_tbl_t;
    typedef eosio::multi_index< "cnyprices"_n, cny_buyorder_t,
        indexed_by<"price"_n, const_mem_fun<cny_buyorder_t, uint64_t, &cny_buyorder_t::by_price> >,
    > sk_tbl_t;


    EOSLIB_SERIALIZE(cny_buyorder_t,    (id)(owner)(started_at)(nickname)(memo)
                                        (quantity)(min_accept_quantity)(price)
                                        (payment_type)(created_at) )
};


/**
 * Sell order with CNY price
 *
 */
struct CONTRACT_TBL cny_sellorder_t {
    uint64_t id;                //PK: available_primary_key
    
    name owner;                 //account
    string nickname;            //online nick
    string memo;                //memo to buyer/sellers

    asset quantity;
    asset min_accept_quantity;
    asset price;                // MGP price the buyer willing to buy, symbol CNY|USD
    uint8_t payment_type;       // 0: bank_transfer; 1: alipay; 2: wechat pay; 3: paypal; 4: master/visa
    time_point created_at;

    cny_sellorder_t() {}
    cny_sellorder_t(uint64_t i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope() const { return 0; }

    uint64_t by_price() const { return price.amount; }
     
    typedef eosio::multi_index< "cnyselorders"_n, cny_sellorder_t> pk_tbl_t;
    typedef eosio::multi_index< "cnyprices"_n, cny_sellorder_t,
        indexed_by<"price"_n, const_mem_fun<cny_sellorder_t, uint64_t, &cny_sellorder_t::by_price>             >,
    > sk_tbl_t;

    EOSLIB_SERIALIZE(cny_sellorder_t,   (id)(owner)(started_at)(nickname)(memo)
                                        (quantity)(min_accept_quantity)(price)
                                        (payment_type)(created_at) )
};

/**
 * Sell order with CNY price
 *
 */
struct CONTRACT_TBL cny_settle_t {
    uint64_t id;                //PK: available_primary_key
    bool buyer_order;           // seller_order when false
    asset buyer;
    asset seller;
    asset quantity;
    asset price;
    
};

}