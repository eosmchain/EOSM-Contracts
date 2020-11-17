#include <eosio.token/eosio.token.hpp>
#include <mgp.bpvoting/bpvoting.hpp>
#include <mgp.bpvoting/mgp_math.hpp>
#include <mgp.bpvoting/utils.h>


using namespace eosio;
using namespace std;
using std::string;

//account: mgp.bpvoting
namespace mgp {

using namespace std;
using namespace eosio;
using namespace wasm::safemath;

/*************** Begin of Helper functions ***************************************/

uint64_t mgp_bpvoting::get_round_id(const time_point& ct) {
	check( time_point_sec(ct) > _gstate.started_at, "too early to start a round" );

	auto elapsed = ct.sec_since_epoch() - _gstate.started_at.sec_since_epoch();
	auto days = elapsed / seconds_per_day;
	return days;
}

void mgp_bpvoting::_current_election_round(const time_point& ct, election_round_t& election_round) {
	election_round.round_id = get_round_id(ct);

	if (!_dbc.get( election_round )) {
		auto days = ct.sec_since_epoch() / seconds_per_day;
		auto start_secs = _gstate.election_round_start_hour * 3600;
		election_round.started_at = time_point() + eosio::seconds(days * seconds_per_day + start_secs);
		election_round.ended_at = election_round.started_at + eosio::seconds(seconds_per_day);
	}
}

void mgp_bpvoting::_list(const name& owner, const asset& quantity, const uint32_t& self_reward_share) {
	check( is_account(owner), owner.to_string() + " not a valid account" );

	candidate_t candidate(owner);
	if ( !_dbc.get(candidate) ) {
		check( quantity >= _gstate.min_bp_list_quantity, "insufficient quantity to list as a candidate" );
		candidate.staked_votes 				= asset(0, SYS_SYMBOL);
		candidate.received_votes 			= asset(0, SYS_SYMBOL);
		candidate.last_claimed_rewards 		= asset(0, SYS_SYMBOL);
		candidate.total_claimed_rewards 	= asset(0, SYS_SYMBOL);
		candidate.unclaimed_rewards			= asset(0, SYS_SYMBOL);
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

	vote_t vote(_self, owner);
	vote.candidate = target;
	vote.quantity = quantity;
	vote.voted_at = ct;
	vote.restarted_at = ct;
	// vote.last_vote_tallied_at = vote.voted_at;
	// vote.last_unvote_tallied_at = vote.voted_at;
	// vote.last_rewarded_at = vote.voted_at;
	_dbc.set( vote );

	voter_t voter(owner);
	if (!_dbc.get( voter )) {
		voter.total_staked = asset(0, SYS_SYMBOL);
		voter.last_claimed_rewards = asset(0, SYS_SYMBOL);
		voter.total_claimed_rewards = asset(0, SYS_SYMBOL);
		voter.unclaimed_rewards = asset(0, SYS_SYMBOL);
	}

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
		vote_t vote(itr->id, itr->owner);
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
	vote_multi_index_tbl unvote_mi(_self, _self.value);
	auto idx = unvote_mi.get_index<"luvtallied"_n>();

	int step = 0;
	for (auto itr = idx.begin(); itr != idx.end(); itr++) {
		if (step++ == _gstate.max_iterate_steps_tally_unvote)
			break;

		if (itr->last_unvote_tallied_at > round.ended_at) {
			round.unvote_tally_completed = true;
			break;
		}
		vote_t vote(itr->id, itr->owner);
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
		vote_t vote(itr->id, itr->owner);
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
		auto ratio = div( coinage.amount, round.total_votes_in_coinage.amount );
		auto bp_rewards = div(mul(_gstate.bp_rewards_per_day, bp.self_reward_share), 10000);
		auto voter_rewards = _gstate.bp_rewards_per_day - bp_rewards;
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
 * Eg: 	"list:6000"			: list self as candidate, taking 60% of rewards to self
 * 		"vote:mgpbpooooo11"	: vote for mgpbpooooo11
 *      ""					: all other cases will be treated as rewards deposit
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
			uint64_t self_reward_share = std::stoul(param);
			check( self_reward_share <= 10000, "self share oversized: " + param);
			check( _gstate.started_at != time_point(), "election not started" );

			_list(from, quantity, self_reward_share);
			_gstate.total_listed += quantity;
			return;

		} else if (cmd == "vote") {	//"vote:$target" (1_coin_1_vote!)
			//vote or revote for a candidate
			check( param.size() < 13, "target name length invalid: " + param );
			name target = name( mgp::string_to_name(param) );
			check( is_account(target), param + " not a valid account" );
			check( quantity >= _gstate.min_bp_vote_quantity, "min_bp_vote_quantity not met" );
			check( _gstate.started_at != time_point(), "election not started" );

			_vote(from, target, quantity);
			_gstate.total_voted += quantity;
			return;

		}
	}

	//all other cases will be handled as rewards
	reward_t reward(_self, _self.value);
	reward.quantity = quantity;
	reward.created_at = current_time_point();
	_dbc.set( reward );

	_gstate.total_rewarded += quantity;

}

/*************** Begin of ACTION functions ***************************************/

/**
 *	ACTION: kick start the election
 */
void mgp_bpvoting::init() {
	require_auth( _self );

	// _gstate = global_t{};
	check (_gstate.started_at == time_point(), "already kickstarted" );

	_gstate.started_at = current_time_point();
}

void mgp_bpvoting::config(
		const uint64_t& max_iterate_steps_tally_vote,
		const uint64_t& max_iterate_steps_tally_unvote,
		const uint64_t& max_iterate_steps_reward,
		const uint64_t& max_bp_size,
		const uint64_t& max_candidate_size,
		const asset& min_bp_list_quantity,
		const asset& min_bp_accept_quantity) {

	require_auth( _self );

	_gstate.max_iterate_steps_tally_vote 	= max_iterate_steps_tally_vote;
	_gstate.max_iterate_steps_tally_unvote 	= max_iterate_steps_tally_unvote;
	_gstate.max_iterate_steps_reward 		= max_iterate_steps_reward;
	_gstate.max_bp_size 					= max_bp_size;
	_gstate.max_candidate_size 				= max_candidate_size;
	_gstate.min_bp_list_quantity 			= min_bp_list_quantity;
	_gstate.min_bp_accept_quantity 			= min_bp_accept_quantity;

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

	auto ct = current_time_point();

	vote_t vote(vote_id, owner);
	check( _dbc.get(vote), "vote not found" );
	check( vote.quantity >= quantity, "unvote overflowed: " + vote.quantity.to_string() );
	auto elapsed = ct.sec_since_epoch() - vote.voted_at.sec_since_epoch();
	check( elapsed > seconds_per_day * 3, "elapsed " + to_string(elapsed) + "sec, not allowed to unvote less than 3 days" );
	vote.unvoted_at = ct;
	_dbc.set(vote);

	voter_t voter(owner);
	check( _dbc.get(voter), "voter not found" );
	check( voter.total_staked >= quantity, "Err: unvote exceeds total staked" );
	voter.total_staked -= quantity;
	_dbc.set(voter);

	_gstate.total_voted -= quantity;

 	{
        token::transfer_action transfer_act{ token_account, { {_self, active_perm} } };
        transfer_act.send( _self, owner, quantity, "unvote" );
    }

}

/**
 * ACTION:	candidate to delist self
 */
void mgp_bpvoting::delist(const name& issuer) {
	require_auth( issuer );

	candidate_t candidate(issuer);
	check( _dbc.get(candidate), issuer.to_string() + " is not a candidate" );
	check( candidate.received_votes.amount == 0, "not fully unvoted" );
	check( candidate.staked_votes.amount > 0, "Err: none staked" );
	_dbc.del( candidate );

	_gstate.total_listed -= candidate.staked_votes;

	auto to_claim = candidate.staked_votes + candidate.unclaimed_rewards;
	{
        token::transfer_action transfer_act{ token_account, { {_self, active_perm} } };
        transfer_act.send( _self, issuer, to_claim, "delist" );
    }

}

/**
 *	ACTION: continuously invoked to execute election until target round is completed
 */
void mgp_bpvoting::execute() {
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

/**
 * ACTION:	voter to claim rewards
 */
void mgp_bpvoting::claimrewards(const name& issuer, const bool is_voter) {
	require_auth( issuer );

	if (is_voter) { //voter
		voter_t voter(issuer);
		check( _dbc.get(voter), "not a voter" );
		check( voter.unclaimed_rewards.amount > 0, "rewards empty" );

		{
			token::transfer_action transfer_act{ token_account, { {_self, active_perm} } };
			transfer_act.send( _self, issuer, voter.unclaimed_rewards , "claim" );
		}

		voter.total_claimed_rewards += voter.unclaimed_rewards;
		voter.last_claimed_rewards = voter.unclaimed_rewards;
		voter.unclaimed_rewards = asset(0, SYS_SYMBOL);
		_dbc.set( voter );

	} else { //candidate
		candidate_t candidate(issuer);
		check( _dbc.get(candidate), "not a candidate" );
		check( candidate.unclaimed_rewards.amount > 0, "rewards empty" );

		{
			token::transfer_action transfer_act{ token_account, { {_self, active_perm} } };
			transfer_act.send( _self, issuer, candidate.unclaimed_rewards , "claim" );
		}

		candidate.total_claimed_rewards += candidate.unclaimed_rewards;
		candidate.last_claimed_rewards = candidate.unclaimed_rewards;
		candidate.unclaimed_rewards = asset(0, SYS_SYMBOL);
		_dbc.set( candidate );

	}

}


}  //end of namespace:: mgpbpvoting