#include <ecoshare/ecoshare.hpp>

using namespace eosio;
using namespace std;
using std::string;

namespace ecoshare {

void mgp_ecoshare::transfer( name from, name to, asset quantity, string memo ){
	require_auth( from );
	
	require_recipient( from );
	require_recipient( to );
	
	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.amount > 0, "Amount quantity must be more then 0");
	if( quantity.symbol != BASE_SYMBOL ){
		check( false, "Token Symbol not allowed");
	}
	check( to == get_self(), "must transfer to mgp_ecoshare contract to stake" );
		
	// 判断是否是第二种支付方式
	if (memo =="mgp+hmio") {
		
		// 添加额度信息
		
	} else {
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

		if (conf->minpay.amount > quantity.amount) {
			check( false, "Amount should be more then ["+to_string(conf->minpay.amount/10000)+"]");
		}
		
		name orderAccount = from;
		
		asset burn;
		burn.amount = ( quantity.amount / 100 ) * conf->destruction;
		burn.symbol = quantity.symbol;
		asset remaining;
		
		remaining.symbol = quantity.symbol;
		
		if (from == SYS_ACCOUNT) {
			orderAccount = name(memo);
			remaining.amount = quantity.amount;

		} else if (from == SHOP_ACCOUNT){
			orderAccount = name(memo);
			remaining.amount = quantity.amount - burn.amount;

		} else {
			remaining.amount = quantity.amount - burn.amount;
		}

		balances balance(get_self(), get_self().value);
		auto bal = balance.find( orderAccount.value );

		if ( bal == balance.end() ) {
			balance.emplace( get_self(), [&]( auto& t ) {
				t.account = orderAccount;
				t.remaining = remaining;
			});
		} else {
			balance.modify( bal, get_self(), [&]( auto& t ) {
				t.remaining.amount += remaining.amount;
			});
		}

		if (from != SYS_ACCOUNT) {
			action(
				permission_level{ get_self(), "active"_n },
				BASE_TRANSFER_FROM, "transfer"_n,
				std::make_tuple( get_self(), "eosio.token"_n, burn, conf->burn_memo)
			).send();
		}
	}
}

void mgp_ecoshare::redeem( name account ){
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
	if(!conf->redeemallow){
		check( false, "Redeem is disabled!");
	}

	balances balance(get_self(), get_self().value);
	auto bal = balance.find( account.value );
	if ( bal == balance.end() ){
		check( false, "Balance is empty!");
	} else {
		if(bal->remaining.amount > 0){
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
}

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

void mgp_ecoshare::bindaddress(name account,string address){
    
    require_auth(account);
    addressbook list(get_self(),get_self().value);
    check(address != "","Address is empty");
    auto iterator = list.find(account.value);
    if (iterator == list.end()) {
        // 进行绑定
        list.emplace(get_self(),[&](auto &row){
            row.account = account;
            row.address = address;
        });
    } else {
        check(false,"The address is already bound");
    }
}


void mgp_ecoshare::delbind(name account,string address){
    require_auth(get_self());

    addressbook list(get_self(),get_self().value);
    check(address != "","Address is empty");
    auto iterator = list.find(account.value);
    if (iterator == list.end()){
        check(false,"not find !");
    }else {
        list.erase(iterator);
    }
}

}

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
	if ( code == BASE_TRANSFER_FROM.value && action == "transfer"_n.value)
	{
		eosio::execute_action( eosio::name(receiver), eosio::name(code), &mgp_ecoshare::transfer );
	}
	else if (code == receiver)
	{
		switch (action)
		{
			EOSIO_DISPATCH_HELPER( mgp_ecoshare, (configure)(redeem)(bindaddress)(delbind))
		}
	}
}