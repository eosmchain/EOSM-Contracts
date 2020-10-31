#include <mgp.ecoshare/ecoshare.hpp>

using namespace eosio;
using namespace std;
using std::string;

namespace mgpecoshare {

void mgp_ecoshare::transfer(name from, name to, asset quantity, string memo){
	require_auth( from );
	
	require_recipient( from );
	require_recipient( to );
	
	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.amount > 0, "Amount quantity must be more then 0" );
	check( quantity.symbol == BASE_SYMBOL, "Token Symbol not allowed" );
	check( to == get_self(), "must transfer to self contract to stake" );
		
	configs config(get_self(), get_self().value);
	auto conf = config.find( get_self().value );
	if( conf == config.end() ){
		asset minpay;
		minpay.amount = 2000000;
		minpay.symbol = quantity.symbol;
		config.emplace( get_self(), [&]( auto& t ) {
			t.account = get_self();
			t.burn_memo = "destruction";
			t.destruction = 50;
			t.redeemallow = 0;
			t.minpay = minpay;
		});
		conf = config.find( get_self().value );
	}
	check( quantity.amount > conf->minpay.amount, 
		"Amount should be more than [" + to_string(conf->minpay.amount/10000) + "]" );
	
	name orderAccount = from;
	
	asset burn;
	burn.amount = ( quantity.amount / 100 ) * conf->destruction;
	burn.symbol = quantity.symbol;

	asset remaining;
	remaining.symbol = quantity.symbol;
	
	if (from == SYS_ACCOUNT){
		orderAccount = name(memo);
		remaining.amount = quantity.amount;

	} else if (from == SHOP_ACCOUNT || from == AGENT_ACCUNT) {
		orderAccount = name(memo);
		remaining.amount = quantity.amount - burn.amount;

	} else {
		remaining.amount = quantity.amount - burn.amount;
	}

	balances balance(get_self(), get_self().value);
	auto bal = balance.find( orderAccount.value );
	if( bal == balance.end() ){
		balance.emplace( get_self(), [&]( auto& t ) {
			t.account = orderAccount;
			t.remaining = remaining;
		});
	} else {
		balance.modify( bal, get_self(), [&]( auto& t ) {
			t.remaining.amount += remaining.amount;
		});
	}
	if (from != SYS_ACCOUNT){
		action(
		permission_level{ get_self(), "active"_n },
		BASE_TRANSFER_FROM, "transfer"_n,
		std::make_tuple( get_self(), "eosio.token"_n, burn, conf->burn_memo)
		).send();
	}
}

[[eosio::action]]
void mgp_ecoshare::configure( string burn_memo, int destruction, bool redeemallow, asset minpay ){
	require_auth(get_self());
	
	configs config(get_self(), get_self().value);
	auto conf = config.find( get_self().value );
	if ( conf == config.end() ) {
		config.emplace( get_self(), [&]( auto& t ) {
			t.account = get_self();
			t.burn_memo = burn_memo;
			t.destruction = destruction;
			t.redeemallow = redeemallow;
			t.minpay = minpay;
		});
	} else {
		config.modify( conf, get_self(), [&]( auto& t ) {
			t.burn_memo = burn_memo;
			t.destruction = destruction;
			t.redeemallow = redeemallow;
			t.minpay = minpay;
		});
	}
}

[[eosio::action]]
void mgp_ecoshare::bindaddress(name account, string address) {
    require_auth(account);

	check(address != "", "Address is empty");

    addressbook book(get_self(), get_self().value);
    auto iterator = book.find(account.value);
	check( iterator == book.end(), "The account is already bound" );

	// 进行绑定
	book.emplace(get_self(), [&](auto &row){
		row.account = account;
		row.address = address;
	});
}

[[eosio::action]]
void mgp_ecoshare::delbind(name account, string address) {
    require_auth(get_self());

    addressbook book(get_self(), get_self().value);
    auto iterator = book.find(account.value);
    check (iterator != book.end(), "account not found!");

    book.erase(iterator);
}


[[eosio::action]]
void mgp_ecoshare::redeem(name account){
	require_auth(account);
	
	configs config(get_self(), get_self().value);
	auto conf = config.find( get_self().value );
	if( conf == config.end() ){
		asset minpay;
		minpay.amount = 2000000;
		minpay.symbol = BASE_SYMBOL;
		config.emplace( get_self(), [&]( auto& t ) {
			t.account = get_self();
			t.burn_memo = "destruction";
			t.destruction = 50;
			t.redeemallow = 0;
			t.minpay = minpay;
		});
	}
	conf = config.find( get_self().value );
	check( conf->redeemallow, "Redeem not allowed!" );

	balances balance(get_self(), get_self().value);
	auto bal = balance.find( account.value );
	check( bal != balance.end(), "Balance not found!");

	if (bal->remaining.amount > 0) {
		action(
			permission_level{ get_self(), "active"_n },
			BASE_TRANSFER_FROM, "transfer"_n,
			std::make_tuple( get_self(), account, bal->remaining, std::string(""))
		).send();
	}

	balance.modify( bal, get_self(), [&]( auto& t ) {
		t.remaining.amount = 0;
	});
}

/**
 * below is added to correct numbers due to historical bugs
 * 
 * this function shall be suppressed unless ultimately necessary for data cleaning purposes
 */
// void ecoshare::reloadnum(const name& from, const name& to, const asset& quant) {
// 	require_auth(get_self());
	
// 	balances balance(get_self(), get_self().value);
// 	auto out_bal = balance.find( from.value );
// 	check( out_bal != balance.end(), "out balance not found!" );

// 	auto to_bal =  balance.find( to.value );
// 	check( to_bal != balance.end(), "to balance not found!" );
			
// 	balance.modify( out_bal, get_self(), [&]( auto& t ) {
// 		t.remaining.amount -= quant.amount;
// 	});
	
// 	balance.modify( to_bal, get_self(), [&]( auto& t ) {
// 		t.remaining.amount += quant.amount;
// 	});
// }

} //end of namespace:: mgpecoshare