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

    template<typename ObjectType>
    bool get(ObjectType& object) {
        typename ObjectType::table_t objects(db_code, db_code.value);
        return( objects.find(object.primary_key()) != objects.end() );
    }

    template<typename ObjectType>
    return_t set(const ObjectType& object) {
        ObjectType obj;
        typename ObjectType::table_t objects(db_code, db_code.value);

        if (objects.find(object.primary_key()) != objects.end()) {
            objects.modify( obj, same_payer, [&]( auto& s ) {
                s = object;
            });
            return return_t::MODIFIED;
        } else {
            objects.emplace( same_payer, [&]( auto& s ) {
                s = object;
            });
            return return_t::APPENDED;
        }
    }

    template<typename ObjectType>
    void del(const ObjectType& object) {
        typename ObjectType::table_t objects(db_code, db_code.value);
        auto itr = objects.find(object.primary_key());
        if ( itr != objects.end() ) {
            objects.erase(itr);
        }
    }

};

}}//db//wasm