#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

namespace wasm { namespace db {

using namespace eosio;

enum return_t{
    NONE    = 0,
    MODIFIED,
    APPENDED,
};

class dbc {
private: 
    name db_code;   //contract owner

public: 
    dbc(const name& code): db_code(code) {}

    template<typename RecordType>
    bool get(RecordType& record) {
        // typename RecordType::table_t tbl(db_code, record.scope());
         typename RecordType::table_t tbl(db_code, db_code.value);
        return( tbl.find(record.primary_key()) != tbl.end() );
    }

    template<typename RecordType>
    bool get_pk() {
        typename RecordType::table_t tbl(db_code, db_code.value);
        return( tbl.available_primary_key() );
    }

    template<typename RecordType>
    auto get_index(const name& index_name) {
        typename RecordType::table_t tbl(db_code, db_code.value);
        auto idx = tbl.get_index<index_name>();
        return idx;
    }

    template<typename RecordType>
    return_t set(const RecordType& record) {
        // typename RecordType::table_t tbl(db_code, record.scope());
        typename RecordType::table_t tbl(db_code, db_code.value);

        auto itr = tbl.find( record.primary_key() );
        if ( itr != tbl.end()) {
            tbl.modify( itr, same_payer, [&]( auto& s ) {
                s = record;
            });
            return return_t::MODIFIED;
        } else {
            tbl.emplace( same_payer, [&]( auto& s ) {
                s = record;
            });
            return return_t::APPENDED;
        }
    }

    template<typename RecordType>
    void del(const RecordType& record) {
        // typename RecordType::table_t tbl(db_code, record.scope());
        typename RecordType::table_t tbl(db_code, db_code.value);
        auto itr = tbl.find(record.primary_key());
        if ( itr != tbl.end() ) {
            tbl.erase(itr);
        }
    }

};

}}//db//wasm