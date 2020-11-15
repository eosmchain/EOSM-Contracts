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

/*************** Begin of Helper functions ***************************************/

uint64_t mgp_bpvoting::get_round_id(const time_point& ct) {
	check( time_point_sec(ct) > _gstate.started_at, "too early to start a round" );

	auto elapsed = ct.sec_since_epoch() - _gstate.started_at.sec_since_epoch();
	auto days = elapsed / seconds_per_day;
	return days;
}

void mgp_bpvoting::_current_election_round(const time_point& ct, election_round_t& election_round) {
	election_round.round_id = get_round_id(ct);
	_dbc.get( election_round );
}

void mgp_bpvoting::_list(const name& owner, const asset& quantity, const uint8_t& self_reward_share) {
	check( is_account(owner), owner.to_string() + " not a valid account" );
	
	candidate_t candidate(owner);
	if ( !_dbc.get(candidate) ) {
		check( quantity.amount >= _gstate.min_bp_list_amount, "insufficient quantity to list as a candidate" );
		candidate.staked_votes 				= asset(0, SYS_SYMBOL);
		candidate.received_votes 			= asset(0, SYS_SYMBOL);
	}
	candidate.self_reward_share 			= self_reward_share;
	candidate.staked_votes 					+= quantity;
	
	_dbc.set( candidate );

}

void mgp_bpvoting::_vote(const name& owner, const name& target, const asset& quantity) {

	candidate_t candidate(target);
	check( _dbc.get(candidate), "candidate ("+target.to_string() +") not listed" );
	candidate.received_votes += quantity;
	_dbc.set(candidate);

	time_point ct = current_time_point();

	vote_t vote;
	vote.id = _dbc.get_pk<vote_t>();
	vote.candidate = target;
	vote.quantity = quantity;
	vote.voted_at = ct;
	vote.restarted_at = ct;
	// vote.last_vote_tallied_at = vote.voted_at;
	// vote.last_unvote_tallied_at = vote.voted_at;
	// vote.last_rewarded_at = vote.voted_at;
	_dbc.set( vote );

	voter_t voter(owner);
	_dbc.get( voter );
	voter.total_staked += quantity;
	_dbc.set( voter );

	election_round_t election_round;
	_current_election_round(ct, election_round);
	election_round.vote_count++;
	_dbc.set( election_round );

}

void mgp_bpvoting::_elect(map<name, asset>& elected_bps, const candidate_t& candidate) {
	elected_bps[candidate.owner] = candidate.received_votes;

	typedef std::pair<name, asset> bp_info_t;
	auto cmp = [](bp_info_t a, bp_info_t b) { return a.second >= b.second; };
	// std::vector<bp_info_t, decltype(cmp)> bps(cmp);
	std::vector<bp_info_t> bps;
	for (auto& it : elected_bps) {
		bps.push_back(it);
	}
	std::sort(bps.begin(), bps.end(), cmp);
	elected_bps.clear();
	auto size = (bps.size() == 22) ? 21 : bps.size();
	for (int i = 0; i < size; i++) {
		elected_bps.emplace(bps[i].first, bps[i].second);
	}
}

void mgp_bpvoting::_tally_votes_for_election_round(election_round_t& round) {
	vote_multi_index_tbl vote_mi(_self, _self.value);
	auto idx = vote_mi.get_index<"lvotallied"_n>();
	int step = 0;
	for (auto itr = idx.begin(); itr != idx.end(); itr++) {
		if (step++ == _gstate.max_iterate_steps_tally_vote)
			break;

		if (itr->last_vote_tallied_at > round.started_at) {
			round.vote_tally_completed = true;
			break;
		}
		vote_t vote(itr->id);
		_dbc.get(vote);
		vote.last_vote_tallied_at = current_time_point();
		_dbc.set(vote);

		candidate_t candidate(itr->candidate);
		check( _dbc.get(candidate), "Err: candidate not found" );
		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		candidate.received_votes += itr->quantity;
		_elect(round.elected_bps, candidate);

		auto age = round.started_at.sec_since_epoch() - itr->restarted_at.sec_since_epoch();
		auto coinage = itr->quantity * age;
		round.total_votes_in_coinage += coinage;
		
	}
	
}

void mgp_bpvoting::_tally_unvotes_for_election_round(election_round_t& round) {
	vote_multi_index_tbl vote_mi(_self, _self.value);
	auto idx = vote_mi.get_index<"luvtallied"_n>();

	int step = 0;
	for (auto itr = idx.begin(); itr != idx.end(); itr++) {
		if (step++ == _gstate.max_iterate_steps_tally_unvote)
			break;

		if (itr->last_unvote_tallied_at > round.started_at) {
			round.unvote_tally_completed = true;
			break;
		}
		vote_t vote(itr->id);
		_dbc.get(vote);
		vote.last_unvote_tallied_at = current_time_point();
		_dbc.set(vote);

		candidate_t candidate(itr->candidate);
		check( _dbc.get(candidate), "Err: candidate not found" );
		check( candidate.received_votes >= itr->quantity, "Err: unvote exceeded" );
		candidate.received_votes -= itr->quantity;
		_elect(round.elected_bps, candidate);

		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		auto age = round.started_at.sec_since_epoch() - itr->restarted_at.sec_since_epoch();
		auto coinage = itr->quantity * age;
		round.total_votes_in_coinage -= coinage;
		
	}
}

void mgp_bpvoting::_reward_through_votes(election_round_t& round) {
	vote_multi_index_tbl vote_mi(_self, _self.value);
	auto idx = vote_mi.get_index<"lastrewarded"_n>();
	int step = 0;
	for (auto itr = idx.begin(); itr != idx.end(); itr++) {
		if (step++ == _gstate.max_iterate_steps_reward)
			break;

		if (itr->last_rewarded_at > round.started_at) {
			round.reward_completed = true;
			break;
		}
		vote_t vote(itr->id);
		_dbc.get(vote);
		vote.last_rewarded_at = current_time_point();
		_dbc.set(vote);

		if (!round.elected_bps.count(itr->candidate)) 
			continue;

		candidate_t bp(itr->candidate);
		check( _dbc.get(bp), "Err: bp not found" );
		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		auto age = round.started_at.sec_since_epoch() - itr->restarted_at.sec_since_epoch();
		auto coinage = itr->quantity * age;
		auto ratio = coinage / round.total_votes_in_coinage;
		auto bp_rewards = rewards_to_bp_per_day * bp.self_reward_share / 10000;
		auto voter_rewards = rewards_to_bp_per_day - bp_rewards;
		bp.unclaimed_rewards += asset(bp_rewards, SYS_SYMBOL);
		voter.unclaimed_rewards += asset(voter_rewards, SYS_SYMBOL);
		
		_dbc.set(bp);
		_dbc.set(voter);
   	}

}

/*************** Begin of eosio.token transfer trigger function ******************/
/** 
 * memo: {$cmd}:{$params} 
 * 
 * Eg: 	"list:6000"			: list self as candidate, sharing 60% out to voters
 * 		"vote:mgpbpooooo11"	: vote for mgpbpooooo11
 * 
 */
void mgp_bpvoting::deposit(name from, name to, asset quantity, string memo) {
	if (to != _self) return;

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "deposit quanity must be positive" );

    std::vector<string> memo_arr = string_split(memo, ':');
    if (memo_arr.size() == 2) {
		string cmd = memo_arr[0];
		string param = memo_arr[1];

		if (cmd == "list") { 		//"list:$share"
			uint64_t voter_reward_share_percent = std::stoul(param);
			check( voter_reward_share_percent <= 10000, "share oversized: " + param);
			check( _gstate.started_at != time_point(), "election not started" );

			_list(from, quantity, voter_reward_share_percent);
			_gstate.total_listed += quantity;
			return;

		} else if (cmd == "vote") {	//"vote:$target" (1_coin_1_vote!)
			//vote or revote for a candidate
			check( param.size() < 13, "target name length invalid: " + param );
			name target = name( mgp::string_to_name(param) );
			check( is_account(target), param + " not a valid account" );
			check( quantity.amount >= min_votes, "less than min_votes" );
			check( _gstate.started_at != time_point(), "election not started" );

			_vote(from, target, quantity);
			_gstate.total_staked += quantity;
			return;

		} 
	}
	
	//all other cases will be handled as rewards
	reward_t reward(_dbc.get_pk<reward_t>());
	reward.quantity = quantity;
	reward.created_at = current_time_point();

	check( false, "id: " + to_string(reward.id) );

	_dbc.set( reward );
	_gstate.total_rewarded += quantity;

}

/*************** Begin of ACTION functions ***************************************/

/**
 *	ACTION: change/move votes from one candidate to another
 *			Internally, it is achieved in two sub-steps:  "unvote" + "vote"
 */
void mgp_bpvoting::chvote(const name& owner, const name& from_candidate, const name& to_candidate, const asset& quantity) {
	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed");
	check( quantity.amount > 0, "chvote quanity must be positive" );

	check( is_account(from_candidate), from_candidate.to_string() + " account invalid" );
	check( is_account(to_candidate), to_candidate.to_string() + " account invalid" );

	auto ct = current_time_point();

	unvote_t unvote;
	unvote.id = _dbc.get_pk<unvote_t>();
	unvote.owner = owner;
	unvote.candidate = from_candidate;
	unvote.quantity = quantity;
	unvote.unvoted_at = ct;
	_dbc.set( unvote );

	vote_t vote;
	vote.id = _dbc.get_pk<unvote_t>();
	vote.owner = owner;
	vote.candidate = to_candidate;
	vote.quantity = quantity;
	vote.voted_at = ct;
	_dbc.set( vote );

}

/**
 *	ACTION: unvote fully or partially
 */
void mgp_bpvoting::unvote(const name& owner, const uint64_t vote_id, const asset& quantity) {
	require_auth( owner );

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "unvote quanity must be positive" );

	vote_t vote(vote_id);
	check( _dbc.get(vote), "vote not found" );
	check( vote.quantity >= quantity, "unvote overflowed: " + vote.quantity.to_string() );

	auto elapsed = current_time_point().sec_since_epoch() - vote.voted_at.sec_since_epoch();
	check( elapsed > seconds_per_day * 3, "not allowed to unvote less than 3 days" );

	unvote_t unvote;
	unvote.id = _dbc.get_pk<unvote_t>();
	unvote.owner = owner;
	unvote.quantity = quantity;
	_dbc.set(unvote);

}

/**
 *	ACTION: continuously invoked to execute election until target round is completed
 */
void mgp_bpvoting::execute(const name& issuer) {
	require_auth( issuer );	//anyone can kick off this action

	auto ct = current_time_point();
	election_round_t curr_round;
	_current_election_round(ct, curr_round);
	auto curr_round_id = curr_round.round_id;
	check( curr_round_id >= 2, "too early to execute election" );

	election_round_t target_round(curr_round_id - 1);
	check( !target_round.execute_completed, "target round is completed" );

	election_round_t last_round(curr_round_id - 2);
	if (_dbc.get(last_round) && !last_round.vote_tally_completed) {
		_tally_votes_for_election_round(last_round);
		_dbc.set(last_round);
	}

	if (_dbc.get(target_round) && !target_round.unvote_tally_completed) {
		_tally_unvotes_for_election_round(target_round);
		_dbc.set(target_round);
	}

	if (target_round.unvote_tally_completed && 
		last_round.vote_tally_completed && 
		!target_round.reward_completed) {
		_reward_through_votes(target_round);
		_dbc.set(target_round);
	}
}


}  //end of namespace:: mgpbpvoting