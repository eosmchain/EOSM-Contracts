#include <eosio.token/eosio.token.hpp>
#include <mgp.vstaking/staking_entities.hpp>
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

 	// _global.remove();

	// seller_t seller("masteraychen"_n);
	// check( _dbc.get(seller), "masteraychen not found in sellers" );
	// _dbc.del(seller);

	_gstate.transaction_fee_receiver = wallet_admin;
	_gstate.min_buy_order_quantity.amount = 10;
	_gstate.min_sell_order_quantity.amount = 10;
	_gstate.min_pos_stake_quantity.amount = 10;
	_gstate.withhold_expire_sec = 30;
	_gstate.pos_staking_contract = "addressbookt"_n;
	_gstate.otc_arbiters.insert( wallet_admin );
}

void mgp_otcstore::setseller(const name& owner, const set<uint8_t>pay_methods, const string& email, const string& memo_to_buyer) {
	require_auth( owner );

	check(email.size() < 64, "email size too large: " + to_string(email.size()) );
	check(memo_to_buyer.size() < max_memo_size, "memo size too large: " + to_string(memo_to_buyer.size()) );

	seller_t seller(owner);
	check( _dbc.get(seller), "seller not found: " + owner.to_string() );

	seller.accepted_payments.clear();
	for (auto& method : pay_methods) {
		check( (PaymentType) method < PaymentType::PAYMAX, "pay method illegal: " + to_string(method) );

		seller.accepted_payments.insert( method );
	}

	seller.email = email;
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
	seller.available_quantity -= quantity;
	_dbc.set( seller );

	if (_gstate.min_pos_stake_quantity.amount > 0) {
		auto staking_con = _gstate.pos_staking_contract;
		balances bal(staking_con, staking_con.value);
		auto itr = bal.find(owner.value);
		check( itr != bal.end(), "POS staking not found for: " + owner.to_string() );
		check( itr->remaining >= _gstate.min_pos_stake_quantity, "POS Staking requirement not met" );
	}

	sell_order_t order(_self, _self.value);
	auto order_id = order.available_primary_key();
	order.emplace( _self, [&]( auto& row ) {
		row.id 					= order_id;
		row.owner 				= owner;
		row.price				= price;
		row.quantity			= quantity;
		row.min_accept_quantity = min_accept_quantity;
		row.closed				= false;
		row.created_at			= time_point_sec(current_time_point());
		row.frozen_quantity.symbol = SYS_SYMBOL;
		row.fulfilled_quantity.symbol = SYS_SYMBOL;
	});
}

void mgp_otcstore::closeorder(const name& owner, const uint64_t& order_id) {
	require_auth( owner );

	seller_t seller(owner);
	check( _dbc.get(seller), "seller not found: " + owner.to_string() ); 

	sell_order_t orders(_self, _self.value);
	auto itr = orders.find(order_id);
	check( itr != orders.end(), "sell order not found: " + to_string(order_id) );
	check( !itr->closed, "order already closed" );
	check( itr->frozen_quantity.amount == 0, "order being processed" );
	check( itr->quantity >= itr-> fulfilled_quantity, "Err: insufficient quanitty" );

	// 撤单后币未交易完成的币退回
	seller.available_quantity += itr -> quantity - itr -> fulfilled_quantity;
	_dbc.set( seller );

	orders.modify( *itr, _self, [&]( auto& row ) {
		row.closed = true;
		row.closed_at = time_point_sec(current_time_point());
	});

}

void mgp_otcstore::opendeal(const name& taker, const uint64_t& order_id, const asset& deal_quantity,const uint64_t& order_sn) {
	require_auth( taker );

	check( deal_quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );

	sell_order_t orders(_self, _self.value);
	auto itr = orders.find(order_id);
	check( itr != orders.end(), "sell order not found: " + to_string(order_id) );
	check( !itr->closed, "order already closed" );
	check( itr->quantity > itr->frozen_quantity, "non-available quantity to deal" );
	check( itr->quantity - itr -> fulfilled_quantity - itr->frozen_quantity >= deal_quantity, "insufficient to deal" );
	///TODO: check if frozen amount timeout already

	asset order_price = itr->price;
	name order_maker = itr->owner;

    sk_deal_t deals(_self, _self.value);
    auto ordersn_index = deals.get_index<"ordersn"_n>();
    auto lower_itr = ordersn_index.lower_bound(order_sn);
    auto upper_itr = ordersn_index.upper_bound(order_sn);
    // for_each(lower_itr,upper_itr,[&](auto& row){
    //     check( false,"order_sn not the only one");
    // });
	check(ordersn_index.find(order_sn) == ordersn_index.end() , "order_sn not the only one");

    auto created_at = time_point_sec(current_time_point());
    auto deal_id = deals.available_primary_key();
    deals.emplace( _self, [&]( auto& row ) {
        row.id 				= deal_id;
        row.order_id 		= order_id;
        row.order_price		= order_price;
        row.deal_quantity	= deal_quantity;
        row.order_maker		= order_maker;
        row.order_taker		= taker;
        row.closed			= false;
        row.created_at		= created_at;
        row.order_sn = order_sn;
		row.expiration_at = time_point_sec(created_at.sec_since_epoch() + _gstate.withhold_expire_sec);
    });

    // 添加交易到期表数据
    exp_tal_t exp_time(_self,_self.value);
    exp_time.emplace( _self, [&]( auto& row ){
        row.deal_id = deal_id;
        row.expiration_at = time_point_sec(created_at.sec_since_epoch() + _gstate.withhold_expire_sec);
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
    check( deal_itr -> order_taker == taker, "no permission");

	auto order_id = deal_itr->order_id;
	sell_order_t orders(_self, _self.value);
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

void mgp_otcstore::passdeal(const name& owner, const uint8_t& user_type, const uint64_t& deal_id, const bool& pass,const uint8_t& pay_type) {
	require_auth( owner );

	sk_deal_t deals(_self, _self.value);
	auto deal_itr = deals.find(deal_id);
	check( deal_itr != deals.end(), "deal not found: " + to_string(deal_id) );
	check( !deal_itr->closed, "deal already closed: " + to_string(deal_id) );

	sell_order_t orders(_self, _self.value);
	auto order_itr = orders.find(deal_itr->order_id);
	check( order_itr != orders.end(), "order not found: " + to_string(deal_itr->order_id) );

	auto now = time_point_sec(current_time_point());

	switch ((UserType) user_type) {
		case MAKER: 
		{
			check( deal_itr->order_maker == owner, "no permission");
			deals.modify( *deal_itr, _self, [&]( auto& row ) {
				row.maker_passed = pass;
				row.maker_passed_at = now;
			});
			break;
		}
		case TAKER:
		{
			exp_tal_t exp_time(_self,_self.value);
			auto exp_itr = exp_time.find(deal_id);
			// 超时表中没有数据表示已经超时
			check( exp_itr != exp_time.end() ,"the order has timed out");
			// 有数据可能是没有及时删除，进行时间检测
			check( exp_itr -> expiration_at > now,"the order has timed out");
			check( deal_itr->order_taker == owner, "no permission");
			check( deal_itr->expiration_at > now,"the order has timed out");
			deals.modify( *deal_itr, _self, [&]( auto& row ) {
				row.taker_passed = pass;
				row.taker_passed_at = now;
				row.pay_type = pay_type;
			});
			break;
		}
		case ARBITER:
		{
			//verify if truly ariter
			check( _gstate.otc_arbiters.count(owner), "not an arbiter: " + owner.to_string() );
			deals.modify( *deal_itr, _self, [&]( auto& row ) {
				row.arbiter = owner;
				row.arbiter_passed = pass;
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

	if (count < 2) return;	//at least 2 persons must have responded

	int maker 	= deal_itr->maker_passed ? 1 : 0;
	int taker 	= deal_itr->taker_passed ? 1 : 0;
	int arbiter = deal_itr->arbiter_passed ? 1 : 0;
	int sum 	= maker + taker + arbiter;

	if (count == 2 && sum == 1) return;	//need arbiter

	if (sum < 2) {	//at least two parties disagreed, deal must be canceled/closed!
		deals.modify( *deal_itr, _self, [&]( auto& row ) {
			row.closed = true;
			row.closed_at = now;
		});

		check( order_itr->frozen_quantity >= deal_itr->deal_quantity, "oversize deal quantity vs fronzen one" );
		orders.modify( *order_itr, _self, [&]( auto& row ) {
			row.frozen_quantity 	-= deal_itr->deal_quantity;
		});

	} else { //at least two parties agreed, hence we can settle now!
		action(
			permission_level{ _self, "active"_n }, token_account, "transfer"_n,
			std::make_tuple( _self, deal_itr->order_taker, deal_itr->deal_quantity,
						std::string("") )
		).send();

		deals.modify( *deal_itr, _self, [&]( auto& row ) {
			row.closed = true;
			row.closed_at = now;
		});

		check( order_itr->frozen_quantity >= deal_itr->deal_quantity, "oversize deal quantity vs fronzen one" );
		orders.modify( *order_itr, _self, [&]( auto& row ) {
			row.frozen_quantity 	-= deal_itr->deal_quantity;
			row.fulfilled_quantity 	+= deal_itr->deal_quantity;

			if (row.fulfilled_quantity == row.quantity) {
				row.closed = true;
				row.closed_at = now;
			}
		});

		seller_t seller(deal_itr->order_maker);
		check( _dbc.get(seller), "Err: seller not found: " + deal_itr->order_maker.to_string() );

		seller.processed_deals++;
		_dbc.set( seller );
	}

}

/**
 *  提取
 * 
 */
void mgp_otcstore::withdraw(const name& owner, asset quantity){
	require_auth( owner );

	check( quantity.amount > 0, "quanity must be positive" );
	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );

	seller_t seller(owner);
	check( _dbc.get(seller), "seller not found: " + owner.to_string() );
	check( seller.available_quantity >= quantity, "The withdrawl amount must be less than the balance" );
	seller.available_quantity -= quantity;
	_dbc.set(seller);
	
	TRANSFER( token_account, owner, quantity, "withdraw" )

}

/**
 * 超时检测
 *
 */
void mgp_otcstore::timeout() {

	sk_deal_t exp_time(_self,_self.value);
	auto now = time_point_sec(current_time_point());
	auto exp_index = exp_time.get_index<"expirationed"_n>();
	auto lower_itr = exp_time.find(now.sec_since_epoch());
	// auto itr = exp_time.begin();

	for(auto itr = lower_itr; itr != exp_index.begin(); --itr){

		if ( itr -> expiration_at <= now ){

			sk_deal_t deals(_self, _self.value);
			auto deal_itr = deals.find(itr -> deal_id);

 			// 订单处于买家未操作状态进行关闭
			if ( deal_itr != deals.end() && !deal_itr->closed &&  !deal_itr -> taker_passed  ){

				auto order_id = deal_itr->order_id;
				sell_order_t orders(_self, _self.value);
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

			exp_time.erase(itr);

	 	}	
	}

}

/*************** Begin of eosio.token transfer trigger function ******************/
/**
 * This happens when a seller deicdes to open sell orders
 */
void mgp_otcstore::deposit(name from, name to, asset quantity, string memo) {
	if (to != _self) return;

	seller_t seller(from);
	_dbc.get( seller );

	if (quantity.symbol == SYS_SYMBOL){
		seller.available_quantity += quantity;
	}
	
	_dbc.set( seller );
	
	
}

/**
 * 进行数据清除，测试用，发布正式环境去除
 */
void mgp_otcstore::deltable(){
	require_auth( _self );

	sell_order_t sellorders(_self,_self.value);
	auto itr = sellorders.begin();
	while(itr != sellorders.end()){
		itr = sellorders.erase(itr);
	}

	sk_deal_t deals(_self,_self.value);
	auto itr1 = deals.begin();
	while(itr1 != deals.end()){
		itr1 = deals.erase(itr1);
	}

	seller_t::tbl_t sellers(_self,_self.value);
	auto itr2 = sellers.begin();
	while(itr2 != sellers.end()){
		itr2 = sellers.erase(itr2);
	}


}


}  //end of namespace:: mgpbpvoting
