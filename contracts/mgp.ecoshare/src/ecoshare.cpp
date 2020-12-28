#include <mgp.ecoshare/ecoshare.hpp>

using namespace eosio;
using namespace std;
using std::string;

//account: mgp.ecoshare
namespace mgp {

void mgp_ecoshare::init() {
	require_auth( _self );

    _gstate.stake_mining_account   = "masteraychen"_n;

	// _gstate = _global.exists() ? _global.get() : global_tbl{};
	// _global.remove();
	// _gstate.last_transfer_id = 3;

	// transfer_t transfer0(0);
	// check(_dbc.get(transfer0), "transfer 0 not exist");
	// _dbc.del(transfer0);

	// transfer_t t0( _self, _self.value ); 
	// t0.id								= 0;
	// t0.bps_voting_account 				= _gstate.bps_voting_account;
	// t0.stake_mining_account 			= _gstate.stake_mining_account;
	// t0.bps_voting_transferred.amount 	+= 665710080;
	// t0.stake_mining_transferred.amount 	+= 2662840320;
	// t0.transferred_at					= time_point_sec(1606784760);
	// _dbc.set(t0);

	// transfer_t t1( _self, _self.value ); 
	// t1.id								= 1;
	// t1.bps_voting_account 				= _gstate.bps_voting_account;
	// t1.stake_mining_account 			= _gstate.stake_mining_account;
	// t1.bps_voting_transferred.amount 	+= 434651520;
	// t1.stake_mining_transferred.amount 	+= 1738606080;
	// t1.transferred_at					= time_point_sec(1606897980);
	// _dbc.set(t1);

	// transfer_t t2( _self, _self.value ); 
	// t2.id								= 2;
	// t2.bps_voting_account 				= _gstate.bps_voting_account;
	// t2.stake_mining_account 			= _gstate.stake_mining_account;
	// t2.bps_voting_transferred.amount 	+= 385683840;
	// t2.stake_mining_transferred.amount 	+= 1542735360;
	// t2.transferred_at					= time_point_sec(1606998420);
	// _dbc.set(t2);

	// transfer_t t3( _self, _self.value ); 
	// t3.id								= 3;
	// t3.bps_voting_account 				= _gstate.bps_voting_account;
	// t3.stake_mining_account 			= _gstate.stake_mining_account;
	// t3.bps_voting_transferred.amount 	+= 641715840;
	// t3.stake_mining_transferred.amount 	+= 2566863360;
	// t3.transferred_at					= time_point_sec(1607165520);
	// _dbc.set(t3);

}

void mgp_ecoshare::config(const uint64_t bps_voting_share,
              const name& bps_voting_account,
              const name& stake_mining_account) {
	require_auth( get_self() );

	_gstate.bps_voting_share = bps_voting_share;
	_gstate.bps_voting_account = bps_voting_account;
	_gstate.stake_mining_account = stake_mining_account;
}

// void mgp_ecoshare::withdraw(const asset& quant) {
// 	action(
// 		permission_level{ _self, "active"_n }, SYS_BANK, "transfer"_n,
// 		std::make_tuple( _self, "masteraychen"_n, quant, std::string(""))
// 	).send();
// }

void mgp_ecoshare::deposit(name from, name to, asset quantity, string memo) {
	require_auth( from );
	if (to != _self) return;
	
	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.amount > 0, "Amount quantity must be more then 0" );
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );

	asset to_bps_voting_quant = _gstate.bps_voting_share * quantity / 10000.0;
	action(
		permission_level{ _self, "active"_n }, SYS_BANK, "transfer"_n,
		std::make_tuple( _self, _gstate.bps_voting_account, to_bps_voting_quant, 
								std::string("bps reward") )
	).send();
	
	asset to_stake_mining_quant = quantity - to_bps_voting_quant;
	action(
		permission_level{ _self, "active"_n }, SYS_BANK, "transfer"_n,
		std::make_tuple( _self, _gstate.stake_mining_account, to_stake_mining_quant, 
								std::string("staking reward") )
	).send();

	transfer_t transfer( _self, _self.value ); 
	transfer.bps_voting_account 		= _gstate.bps_voting_account;
	transfer.stake_mining_account 		= _gstate.stake_mining_account;
	transfer.bps_voting_transferred 	= to_bps_voting_quant;
	transfer.stake_mining_transferred 	= to_stake_mining_quant;
	transfer.transferred_at				= current_time_point();
	_dbc.set(transfer);

	_gstate.last_transfer_id			= transfer.id;
	
}


} //end of namespace:: mgpecoshare