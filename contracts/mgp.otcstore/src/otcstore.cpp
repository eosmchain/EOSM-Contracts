#include <eosio.token/eosio.token.hpp>
#include <mgp.otcstore/otcstore.hpp>
#include <mgp.otcstore/mgp_math.hpp>
#include <mgp.otcstore/utils.h>


using namespace eosio;
using namespace std;
using std::string;

namespace mgp {

using namespace std;
using namespace eosio;
using namespace wasm::safemath;

void mgp_otcstore::init() {
	auto wallet_admin = "mwalletadmin"_n;

	_gstate.transaction_fee_receiver = wallet_admin;
	_gstate.min_buy_order_quantity.amount = 10;
	_gstate.min_sell_order_quantity.amount = 10;
	_gstate.min_pos_stake_quantity.amount = 0;
	_gstate.otc_arbiters.insert( wallet_admin );
}

void mgp_otcstore::setseller(const name& owner, const set<PaymentType>pay_methods, const string& memo_to_buyer) {
	require_auth( owner );

	check(memo_to_buyer.size() < max_memo_size, "memo size too large: " + to_string(memo_to_buyer.size()) );

	seller_t seller(owner);
	check( _dbc.get(seller), "seller not found: " + owner.to_string() );

	seller.accepted_payments.clear();
	for (auto& method : pay_methods) {
		check( method < PaymentType::PAYMAX, "pay method illegal: " + to_string(method) );

		seller.accepted_payments.insert( method );
	}
	
	seller.memo = memo_to_buyer;

	_dbc.set( seller );
	
}

/**
 * only seller allowed to open orders
 */
void mgp_otcstore::openorder(const name& owner, const asset& quantity, const asset& price, const asset& min_accept_quantity) {
	require_auth( owner );

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "order quanity must be positive" );

	check( price.symbol.is_valid(), "Invalid quantity symbol name" );
	check( price.is_valid(), "Invalid quantity");
	check( price.symbol == CNY_SYMBOL, "Price Symbol not allowed" );
	check( price.amount > 0, "price must be positive" );

	check( min_accept_quantity.symbol == price.symbol, "min accept symbol not equal to price symbol" );

	seller_t seller(owner);
	check( _dbc.get(seller), "seller not found: " + owner.to_string() ); 
	check( seller.available_quantity >= quantity, "seller insufficient quantitiy to sell" );

	sk_sellorder_t order(_self, _self.value);
	auto order_id = order.available_primary_key();
	order.emplace( _self, [&]( auto& row ) {
		row.id 					= order_id;
		row.owner 				= owner;
		row.price				= price;
		row.quantity			= quantity;
		row.min_accept_quantity = min_accept_quantity;
		row.closed				= false;
		row.created_at			= time_point_sec(current_time_point());
	});

	seller.available_quantity -= quantity;
	_dbc.set( seller );

}

void mgp_otcstore::closeorder(const name& owner, const uint64_t& order_id) {
	require_auth( owner );

	seller_t seller(owner);
	check( _dbc.get(seller), "seller not found: " + owner.to_string() ); 

	sk_sellorder_t orders(_self, _self.value);
	auto itr = orders.find(order_id);
	check( itr != orders.end(), "sell order not found: " + to_string(order_id) );
	check( !itr->closed, "order already closed" );
	check( itr->frozen_quantity.amount == 0, "order being processed" );

	orders.modify( *itr, _self, [&]( auto& row ) {
		row.closed = true;
		row.closed_at = time_point_sec(current_time_point());
	});

}

void mgp_otcstore::opendeal(const name& taker, const uint64_t& order_id, const asset& deal_quantity) {
	require_auth( taker );

	check( deal_quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );

	sk_sellorder_t orders(_self, _self.value);
	auto itr = orders.find(order_id);
	check( itr != orders.end(), "sell order not found: " + to_string(order_id) );
	check( !itr->closed, "order already closed" );
	check( itr->quantity > itr->frozen_quantity, "non-available quantity to deal" );
	check( itr->quantity - itr->frozen_quantity >= deal_quantity, "insufficient to deal" );
	///TODO: check if frozen amount timeout already

	asset order_price = itr->price;
	name order_maker = itr->owner;

	sk_deal_t deals(_self, _self.value);
	auto deal_id = deals.available_primary_key();
	deals.emplace( _self, [&]( auto& row ) {
		row.id 				= deal_id;
		row.order_id 		= order_id;
		row.order_price		= order_price;
		row.deal_quantity	= deal_quantity;
		row.order_maker		= order_maker;
		row.order_taker		= taker;
		row.closed			= false;
		row.created_at		= time_point_sec(current_time_point());
	});

	orders.modify( *itr, _self, [&]( auto& row ) {
		row.frozen_quantity += deal_quantity;
	});
}

/**
 * actively close the deal by order taker
 */
void mgp_otcstore::closedeal(const name& taker, const uint64_t& deal_id) {
	require_auth( taker );

	sk_deal_t deals(_self, _self.value);
	auto deal_itr = deals.find(deal_id);
	check( deal_itr != deals.end(), "deal not found: " + to_string(deal_id) );
	check( !deal_itr->closed, "deal already closed: " + to_string(deal_id) );

	auto order_id = deal_itr->order_id;
	sk_sellorder_t orders(_self, _self.value);
	auto order_itr = orders.find(order_id);
	check( order_itr != orders.end(), "sell order not found: " + to_string(order_id) );
	check( !order_itr->closed, "order already closed" );

	auto deal_quantity = deal_itr->deal_quantity;
	check( order_itr->frozen_quantity >= deal_quantity, "Err: order frozen quantity smaller than deal quantity" );

	orders.modify( *order_itr, _self, [&]( auto& row ) {
		row.frozen_quantity -= deal_quantity;
	});

	deals.modify( *deal_itr, _self, [&]( auto& row ) {
		row.closed = true;
		row.closed_at = time_point_sec(current_time_point());
	});

}

void mgp_otcstore::passdeal(const name& owner, const UserType& user_type, const uint64_t& deal_id, const bool& pass) {
	require_auth( owner );

	sk_deal_t deals(_self, _self.value);
	auto deal_itr = deals.find(deal_id);
	check( deal_itr != deals.end(), "deal not found: " + to_string(deal_id) );
	check( !deal_itr->closed, "deal already closed: " + to_string(deal_id) );

	auto now = time_point_sec(current_time_point());

	switch (user_type) {
		MAKER: 
		{
			deals.modify( *deal_itr, _self, [&]( auto& row ) {
				row.maker_passed = true;
				row.maker_passed_at = now;
			});	
			break;
		}
		TAKER: 
		{
			deals.modify( *deal_itr, _self, [&]( auto& row ) {
				row.taker_passed = true;
				row.taker_passed_at = now;
			});	
			break;
		}
		ARBITER: 
		{
			//verify if truly ariter
			check( _gstate.otc_arbiters.count(owner), "not an arbiter: " + owner.to_string() );
			deals.modify( *deal_itr, _self, [&]( auto& row ) {
				row.arbiter_passed = true;
				row.arbiter_passed_at = now;
			});	
			break;
		}
		default:
			break;
	} 

	int count = 0;
	if (deal_itr->maker_passed_at != time_point_sec())
		count++;

	if (deal_itr->taker_passed_at != time_point_sec())
		count++;

	if (deal_itr->arbiter_passed_at != time_point_sec())
		count++;
	
	if (count < 2) return;

	int maker = deal_itr->maker_passed ? 1 : 0;
	int taker = deal_itr->taker_passed ? 1 : 0;
	int arbiter = deal_itr->arbiter_passed ? 1 : 0;
	int sum = maker + taker + arbiter;

	if (count == 2) {
		if (sum == 2) {
			//TODO: settle now
		} else
			return;	

	} else if (count == 3) { //arbitraged
		if (sum >= 2) {
			//TODO: settle now
		} else {
			//TODO: to cancel the deal
		}
	} 

}

/*************** Begin of eosio.token transfer trigger function ******************/
/**
 * This happens when a seller deicdes to open sell orders
 */
void mgp_otcstore::deposit(name from, name to, asset quantity, string memo) {
	if (to != _self) return;

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "deposit quanity must be positive" );

	seller_t seller(from);
	_dbc.get( seller );
	seller.available_quantity += quantity;
	_dbc.set( seller );
	
}

}  //end of namespace:: mgpbpvoting