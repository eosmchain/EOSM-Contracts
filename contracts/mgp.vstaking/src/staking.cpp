#include <mgp.vstaking/staking.hpp>
#include <eosio.token/eosio.token.hpp>

using namespace eosio;
using namespace std;
using std::string;

/// REF OLD VERSION: 
///      https://gitlab.com/mgpchain/mgp-contracts/-/blob/master/smart_mgp.cpp
///

//deployed account: addressbookt
namespace mgp {

void smart_mgp::transfer(name from, name to, asset quantity, string memo){
	require_auth( from );
	if (to != _self) return;
	
	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );

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
		check( false, "Amount should be more than [" + to_string(conf->minpay.amount/10000) + "]" );
	}
	
	asset to_burn_quant(0, SYS_SYMBOL);
	to_burn_quant.amount = ( quantity.amount / 100 ) * conf->destruction;
	asset remaining(0, SYS_SYMBOL);
	
	if (from == AGENT_ACCOUNT) {
		size_t pos;
		// 解析memo数据：接收者:销毁数:总MGP数
		std::vector<string> memo_arr = string_split(memo, ':');
		check(memo_arr.size() == 3,"Err: Parsing failed");
		from = name(memo_arr[0]);
		check(is_account(from),"It's not an account.");
		//print(stol(memo_arr[1],&pos));
		to_burn_quant.amount = stol(memo_arr[1],&pos);

	}else {
		if (memo != "" && is_account(name(memo))) {
			from = name(memo);
		}
	} 

	remaining = quantity - to_burn_quant;

	check(to_burn_quant <= quantity, "The number of destruction is incorrect.");
	check(remaining <= quantity, "The data is incorrect.");

	balances balance( _self, _self.value);
	auto bal = balance.find( from.value );
	if( bal == balance.end() ){
		balance.emplace( get_self(), [&]( auto& t ) {
			t.account = from;
			t.remaining = remaining;
		});
	} else {
		balance.modify( bal, get_self(), [&]( auto& t ) {
			t.remaining.amount += remaining.amount;
		});
	}

	if (to_burn_quant.amount > 0) {
		BURN( SYS_BANK, _self, to_burn_quant, std::string("staking burn") )
	}
}

void smart_mgp::configure( string burn_memo, int destruction, bool redeemallow, asset minpay ){
	require_auth( _self );
	
	configs config(get_self(), get_self().value);
	auto conf = config.find( get_self().value );
	if( conf == config.end() ){
		config.emplace( get_self(), [&]( auto& t ) {
			t.account = get_self();
			t.burn_memo = burn_memo;
			t.destruction = destruction;
			t.redeemallow = redeemallow;
			t.minpay = minpay;
		});
	}else{
	config.modify( conf, get_self(), [&]( auto& t ) {
		t.burn_memo = burn_memo;
		t.destruction = destruction;
		t.redeemallow = redeemallow;
		t.minpay = minpay;
	});
}

     
}

void smart_mgp::encorrection( bool enable_data_correction ) {
	require_auth( _self );

	configs2_tbl conf(_self, _self.value);
	auto itr = conf.find(_self.value);
    if (itr == conf.end()){
        conf.emplace(get_self(),[&](auto &row){
			row.account = _self;
            row.data_correction_enabled = enable_data_correction;
        });
    }else {
        conf.modify(itr, get_self(), [&](auto &row){
            row.data_correction_enabled = enable_data_correction;
        });
    }

}

void smart_mgp::bindaddress(const name& account, const string& address) {
    require_auth( account );

  	addressbook list(get_self(),get_self().value);
    check(address != "","Address is empty");
    auto iterator = list.find(account.value);
    if (iterator == list.end()){
        // 进行绑定
        list.emplace(get_self(),[&](auto &row){
            row.account = account;
            row.address = address;
        });
    }else {
        check(false,"The address is already bound");
    }

}

void smart_mgp::delbind(const name& account, const string& address) {
    require_auth( account );

   	addressbook list(get_self(),get_self().value);
    check(address != "","Address is empty");
    auto iterator = list.find(account.value);
    if (iterator == list.end()){
        check(false,"not find !");
    }else {
        list.erase(iterator);
    }

	
}

void smart_mgp::redeem(name account){
	require_auth( account );
	
	configs config(get_self(), get_self().value);
	auto conf = config.find( get_self().value );
	if( conf == config.end() ){
		asset minpay;
		minpay.amount = 2000000;
		minpay.symbol = SYS_SYMBOL;
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
	if( bal == balance.end() ){
		check( false, "Balance is empty!");
	}else{
		if(bal->remaining.amount > 0){
			action(
				permission_level{ get_self(), "active"_n }, SYS_BANK, "transfer"_n,
				std::make_tuple( get_self(), account, bal->remaining, std::string(""))
			).send();
		}
		balance.modify( bal, get_self(), [&]( auto& t ) {
			t.remaining.amount = 0;
		});
	}
}

/**
 * below is added to correct numbers due to historical bugs
 * 
 * this function shall be suppressed unless ultimately necessary for data cleaning purposes
 */
void smart_mgp::reloadnum(const name& from, const name& to, const asset& quant) {
	require_auth(get_self());
	
	configs2_tbl configs2(_self, _self.value);
	auto conf = configs2.find(_self.value);
	check( conf != configs2.end(), "configs.ext not set" );
	check( conf->data_correction_enabled, "data correction disabled" );

	balances balance(get_self(), get_self().value);
	auto out_bal = balance.find( from.value );
	auto to_bal =  balance.find( to.value );
	if( out_bal == balance.end() || to_bal == balance.end()){
		check( false, "Balance is empty!");
	}else{
			
		balance.modify( out_bal, get_self(), [&]( auto& t ) {
			t.remaining.amount -= quant.amount;
		});
		
		balance.modify( to_bal, get_self(), [&]( auto& t ) {
			t.remaining.amount += quant.amount;
		});
	}
}

} //end of namespace:: mgpecoshare
