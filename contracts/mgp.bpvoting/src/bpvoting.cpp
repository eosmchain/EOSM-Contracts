#include <mgp.bpvoting/bpvoting.hpp>
#include <eosio.token/eosio.token.hpp>
#include "mgp.bpvoting/utils.h"

using namespace eosio;
using namespace std;
using std::string;

//account: mgp.bpvoting
namespace mgp {

using namespace std;
using namespace eosio;

/** 
 * memo: {$cmd}:{$params} 
 * 
 * Eg: 	"list:6000"			: list self as candidate, sharing 60% out to voters
 * 		"vote:mgpbpooooo11"	: vote for mgpbpooooo11
 * 
 */
void mgp_bpvoting::transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
	require_auth( from );
	if (to != _self) return;

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "unvote quanity must be positive" );

    std::vector<string> memo_arr = string_split(memo, ':');
    check( memo_arr.size() == 2, "memo error" );
	string cmd = memo_arr[0];
	string param = memo_arr[1];

	if (cmd == "list") { 		//"list:$share"
		uint64_t voter_reward_share_percent = std::stoul(param);
		check( voter_reward_share_percent <= 10000, "share oversized: " + param);
		process_list(from, quantity, voter_reward_share_percent);

		_gstate.total_listed += quantity;

	} else if (cmd == "vote") {	//"vote:$target" (1coin:1vote!)
		//vote or revote for a candidate
		check( param.size() < 13, "target name invalid: " + param );
		name target = name( mgp::string_to_name(param) );
		check( is_account(target), param + " not a valid account" );
		process_vote(from, target, quantity);

		_gstate.total_staked += quantity;

	} else { //rewards
		// accepted as the rewards for voters and bps
		_gstate.total_rewarded += quantity;
	}
}

void mgp_bpvoting::process_list(const name& owner, const asset& quantity, const uint8_t& voter_reward_share_percent) {
	check( quantity.amount >= _gstate.min_bp_list_amount, "insufficient quantity to list as a candidate" );

	candidate_t candidate(owner);
	check( !_dbc.get(candidate), "candidate already listed" );

	candidate.voter_reward_share_percent = voter_reward_share_percent;
	candidate.staked_votes = quantity;
	candidate.staked_votes = asset(0, SYS_SYMBOL);
	candidate.received_votes = asset(0, SYS_SYMBOL);
	_dbc.set( candidate );

}

void mgp_bpvoting::process_vote(const name& owner, const name& target, const asset& quantity) {

	candidate_t candidate(target);
	check( _dbc.get(candidate), "candidate ("+target.to_string() +") not listed" );
	candidate.received_votes += quantity;
	_dbc.set(candidate);

	voter_t voter(owner);
	_dbc.get( voter );
	voter.votes[ target ] = vote_info( quantity, current_time_point() );
	check( voter.votes.size() <= _gstate.max_candidate_size, "voted candidates oversized" );
	voter.total_staked += quantity;
	_dbc.set( voter );

}

/**
 *	ACTION: change/move votes from one candidate to another
 */
void mgp_bpvoting::chvote(const name& owner, const name& from_candidate, const name& to_candidate, const asset& quantity) {
	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed");
	check( quantity.amount > 0, "chvote quanity must be positive" );

	check( is_account(from_candidate), from_candidate.to_string() + " account invalid" );
	check( is_account(to_candidate), to_candidate.to_string() + " account invalid" );

	voter_t voter(owner);
	check( _dbc.get(voter), owner.to_string() + " is not a voter" );
	check( voter.votes.find(from_candidate) != voter.votes.end(), from_candidate.to_string() +  " not a candidate" );
	check( voter.votes[from_candidate].quantity >= quantity, "over move from_candidate (" + voter.votes[from_candidate].quantity.to_string() + ")");
	voter.votes[from_candidate].quantity -= quantity;
	voter.votes[to_candidate].quantity += quantity;
	
	_dbc.set( voter );

}

/**
 *	ACTION: unvote fully or partially
 */
void mgp_bpvoting::unvote(const name& owner, const name& target, const asset& quantity) {
	require_auth( owner );

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "unvote quanity must be positive" );

	voter_t voter(owner);
	check( _dbc.get(voter), "voter (" + owner.to_string() + ") not found" );
	check( voter.total_staked >= quantity, "unvote over staked: " + voter.total_staked.to_string() );
	check( voter.votes.find(target) != voter.votes.end(), "candidate (" + target.to_string() + ") not found" );
	check( voter.votes[target].quantity >= quantity, "unvote over voted: " + voter.votes[target].quantity.to_string() );

	auto elapsed = current_time_point().sec_since_epoch() - voter.votes[target].voted_at.sec_since_epoch();
	check( elapsed > seconds_per_day * 3, "not allowed to unvote less than 3 days" );

	token::transfer_action transfer_act{ token_account, { {_self, active_perm} } };
	transfer_act.send( _self, owner, quantity, "unvote" );

	voter.votes[target].quantity -= quantity;
	if (voter.votes[target].quantity.amount == 0)
		voter.votes.erase(target);
    
	voter.total_staked -= quantity;	
	_dbc.set( voter );

	candidate_t candidate(target);
	check( _dbc.get(candidate), "candidate (" + target.to_string() + ") not found" );
	check( candidate.received_votes >= quantity, "votes data is dirty" );
	candidate.received_votes -= quantity;
	_dbc.set( candidate );

	_gstate.total_staked -= quantity;
}

//TODO reorder candidates to update bps
// could be invoked by BP onblock action
void mgp_bpvoting::updatebps() {

}

}  //end of namespace:: mgpbpvoting