#pragma once

#include <eosio/eosio.hpp>
#include "dex_const.hpp"
#include "dex_states.hpp"
#include "match.hpp"

using namespace std;
using namespace eosio;

class DEX_CONTRACT mgp_dex : public contract {
public:
    using contract::contract;

public:
    mgp_dex(name receiver, name code, datastream<const char *> ds)
        : contract(receiver, code, ds), _conf_tbl(get_self(), get_self().value),
          _global(dex::global_state::make_global(get_self())) {
        _config = _conf_tbl.exists() ? _conf_tbl.get() : get_default_config();
    }

    ~mgp_dex() {
        _global->save(get_self());
    }

    [[eosio::action]] 
    void setconfig(const dex::config &conf);

    [[eosio::action]] 
    void setsympair(const symbol &asset_symbol, const symbol &coin_symbol,
                    const asset &min_asset_quant, const asset &min_coin_quant, bool enabled);

    //make orders
    [[eosio::on_notify("*::transfer")]] 
    void ontransfer(name from, name to, asset quantity, string memo);

    // TODO: const list<uint64_t> &sym_pairs
    /**
     *  @param max_count the max count of match item
     *  @param sym_pairs the symol pairs to match. is empty, match all
     */
    [[eosio::action]] void match(uint32_t max_count, const vector<uint64_t> &sym_pairs);

    [[eosio::action]] void cancel(const uint64_t &order_id);

    using setconfig_action  = action_wrapper<"setconfig"_n,     &mgp_dex::setconfig>;
    using setsympair_action = action_wrapper<"setsympair"_n,    &mgp_dex::setsympair>;
    using ontransfer_action = action_wrapper<"ontransfer"_n,    &mgp_dex::ontransfer>;
    using match_action      = action_wrapper<"match"_n,         &mgp_dex::match>;
    using cancel_action     = action_wrapper<"cancel"_n,        &mgp_dex::cancel>;

private:
    dex::config get_default_config();
    void process_refund(dex::order_t &buy_order);
    void match_sym_pair(const dex::symbol_pair_t &sym_pair, uint32_t max_count, uint32_t &matched_count);

    dex::config_table           _conf_tbl;
    dex::config                 _config;
    dex::global_state::ptr_t    _global;
};
