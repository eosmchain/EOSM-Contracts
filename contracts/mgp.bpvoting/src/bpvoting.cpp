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
	auto rounds = elapsed / _gstate.election_round_sec;
	return rounds; //usually in days
}

void mgp_bpvoting::_current_election_round(const time_point& ct, election_round_t& curr_round) {
	auto round_id = get_round_id(ct);
	curr_round.round_id = round_id;

	if (!_dbc.get( curr_round )) {
		election_round_t last_round(_gstate.last_election_round);
		check( _dbc.get(last_round), "Err: last round not found" );
		last_round.next_round_id = curr_round.round_id;
		_dbc.set( last_round );

		curr_round.started_at = last_round.ended_at;
		auto elapsed = ct.sec_since_epoch() - last_round.ended_at.sec_since_epoch();
		auto rounds = elapsed / _gstate.election_round_sec;
		curr_round.ended_at = curr_round.started_at + time_point(eosio::seconds(rounds * _gstate.election_round_sec));
		_dbc.set( curr_round );

		_gstate.last_election_round = round_id;
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

	// vote.last_vote_tallied_at = vote.voted_at;
	// vote.last_unvote_tallied_at = vote.voted_at;
	// vote.last_rewarded_at = vote.voted_at;

	vote_tbl votes(_self, _self.value);
	votes.emplace( _self, [&]( auto& row ) {
		row.id = votes.available_primary_key();
		row.owner = owner;
		row.candidate = target;
		row.quantity = quantity;
		row.voted_at = ct;
		row.restarted_at = ct;
	});

	voter_t voter(owner);
	if (!_dbc.get( voter )) {
		voter.total_staked = asset(0, SYS_SYMBOL);
		voter.last_claimed_rewards = asset(0, SYS_SYMBOL);
		voter.total_claimed_rewards = asset(0, SYS_SYMBOL);
		voter.unclaimed_rewards = asset(0, SYS_SYMBOL);
	}

	voter.total_staked += quantity;
	_dbc.set( voter );

	election_round_t curr_round;
	_current_election_round(ct, curr_round);
	curr_round.vote_count++;
	curr_round.total_votes += quantity;
	_dbc.set( curr_round );

}

void mgp_bpvoting::_elect(map<name, asset>& elected_bps, const candidate_t& candidate) {
	elected_bps[candidate.owner] = candidate.received_votes;

	typedef std::pair<name, asset> bp_info_t;
	// std::vector<bp_info_t, decltype(cmp)> bps(cmp);
	std::vector<bp_info_t> bps;
	for (auto& it : elected_bps) {
		bps.push_back(it);
	}

	auto cmp = [](bp_info_t a, bp_info_t b) { return a.second >= b.second; };
	std::sort(bps.begin(), bps.end(), cmp);
	elected_bps.clear();
	auto size = (bps.size() == 22) ? 21 : bps.size();
	for (int i = 0; i < size; i++) {
		elected_bps.emplace(bps[i].first, bps[i].second);
	}
}

void mgp_bpvoting::_tally_votes_for_last_round(election_round_t& round) {
	vote_tbl votes(_self, _self.value);
	auto idx = votes.get_index<"lvotallied"_n>();
	auto lower_itr = idx.lower_bound( uint64_t(round.started_at.sec_since_epoch()) ); 
	auto upper_itr = idx.upper_bound( uint64_t(round.ended_at.sec_since_epoch()) ); 
	int step = 0;

	bool completed = true;
	for (auto itr = lower_itr; itr != upper_itr && itr != idx.end(); itr++) {
		if (step++ == _gstate.max_tally_vote_iterate_steps) {
			completed = false;
			break;
		}
		auto v_itr = votes.find(itr->id);
		check( v_itr != votes.end(), "Err: vote[" + to_string(itr->id) + "] not found" );
		votes.modify( v_itr, _self, [&]( auto& row ) {
      		row.last_vote_tallied_at = current_time_point();
   		});

		candidate_t candidate(itr->candidate);
		check( _dbc.get(candidate), "Err: candidate not found" );
		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		candidate.tallied_votes += itr->quantity;
		if (candidate.staked_votes + candidate.tallied_votes >= _gstate.min_bp_accept_quantity)
			_elect(round.elected_bps, candidate);

		auto age = round.started_at.sec_since_epoch() - itr->restarted_at.sec_since_epoch();
		auto coinage = itr->quantity * age;
		round.total_votes_in_coinage += coinage;		
	}

	round.vote_tally_completed = completed;
	_dbc.set( round );

}

void mgp_bpvoting::_tally_unvotes_for_target_round(election_round_t& round) {
	vote_tbl votes(_self, _self.value);
	auto idx = votes.get_index<"luvtallied"_n>();
	auto lower_itr = idx.lower_bound( uint64_t(round.started_at.sec_since_epoch()) ); 
	auto upper_itr = idx.upper_bound( uint64_t(round.ended_at.sec_since_epoch()) ); 
	int step = 0;

	bool completed = true;
	string ids = "";
	for (auto itr = lower_itr; itr != upper_itr && itr != idx.end(); itr++) {
		if (step++ == _gstate.max_tally_unvote_iterate_steps) {
			completed = false;
			break;
		}
		ids += itr->id + ", ";

		auto v_itr = votes.find(itr->id);
		votes.modify( v_itr, _self, [&]( auto& row ) {
      		row.last_unvote_tallied_at = current_time_point();
   		});
		   
		candidate_t candidate(itr->candidate);
		check( _dbc.get(candidate), "Err: candidate not found" );
		check( candidate.tallied_votes >= itr->quantity, "Err: unvote exceeded" );
		candidate.tallied_votes -= itr->quantity;
		_elect(round.elected_bps, candidate);

		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		auto age = round.started_at.sec_since_epoch() - itr->restarted_at.sec_since_epoch();
		auto coinage = itr->quantity * age;
		round.total_votes_in_coinage -= coinage;
		round.unvote_count++;

	}

	check( false, ids );
	
	round.unvote_tally_completed = completed;

	_dbc.set( round  );

}

void mgp_bpvoting::_reward_through_votes(election_round_t& round) {
	vote_tbl votes(_self, _self.value);
	auto idx = votes.get_index<"lastrewarded"_n>();
	auto upper_itr = idx.upper_bound( uint64_t(round.started_at.sec_since_epoch()) ); 
	int step = 0;

	check( round.elected_bps.size() > 0, "none elected for round[" + to_string(round.round_id) + "]" );
	auto per_bp_rewards = div( _gstate.available_rewards.amount, round.elected_bps.size() );

	bool completed = true;
	for (auto itr = idx.begin(); itr != upper_itr && itr != idx.end(); itr++) {
		if (step++ == _gstate.max_reward_iterate_steps) {
			completed = false;
			break;
		}

		auto v_itr = votes.find(itr->id);
		votes.modify( v_itr, _self, [&]( auto& row ) {
      		row.last_rewarded_at = current_time_point();
   		});

		if (itr->unvoted_at < round.ended_at)
			continue;	//skip unovted before new round

		if (!round.elected_bps.count(itr->candidate))
			continue;	//skip vote with its canddiate unelected

		candidate_t bp(itr->candidate);
		check( _dbc.get(bp), "Err: bp not found" );
		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		auto elapsed = round.started_at.sec_since_epoch() - itr->voted_at.sec_since_epoch();
		auto mons = elapsed % (30 * seconds_per_day);
		votes.modify( v_itr, _self, [&]( auto& row ) {
      		row.restarted_at += microseconds(30 * mons * seconds_per_day * 1000'000ll);
   		});
		auto age = round.started_at.sec_since_epoch() - itr->restarted_at.sec_since_epoch();
		auto coinage = itr->quantity * age;
		auto ratio = div( coinage.amount, round.total_votes_in_coinage.amount );
		auto bp_rewards = div( mul(per_bp_rewards, bp.self_reward_share), share_boost );
		auto voter_rewards = mul( ratio, per_bp_rewards - bp_rewards );
		bp.unclaimed_rewards += asset(bp_rewards, SYS_SYMBOL);
		voter.unclaimed_rewards += asset(voter_rewards, SYS_SYMBOL);

		_dbc.set(bp);
		_dbc.set(voter);

   	}

	round.reward_completed = completed;
	_dbc.set( round );

	if (completed)
		_gstate.last_execution_round = round.round_id;

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

	_gstate.total_received_rewards += quantity;
	_gstate.available_rewards += quantity;

}

/*************** Begin of ACTION functions ***************************************/

/**
 *	ACTION: kick start the election
 */
void mgp_bpvoting::init() {
	require_auth( _self );

	check (_gstate.started_at == time_point(), "already kickstarted" );

	auto ct = current_time_point();
	_gstate.started_at = ct;
	_gstate.last_election_round = 0;

	auto days = ct.sec_since_epoch() / seconds_per_day;
	auto start_secs = _gstate.election_round_start_hour * 3600;

	election_round_t election_round(0);
	election_round.started_at = time_point() + eosio::seconds(days * seconds_per_day + start_secs);
	election_round.ended_at = election_round.started_at + eosio::seconds(_gstate.election_round_sec);
	_dbc.set( election_round );

}

void mgp_bpvoting::config(
		const uint64_t& max_tally_vote_iterate_steps,
		const uint64_t& max_tally_unvote_iterate_steps,
		const uint64_t& max_reward_iterate_steps,
		const uint64_t& max_bp_size,
		const uint64_t& election_round_sec,
		const uint64_t& refund_delay_sec,
		const uint64_t& election_round_start_hour,
		const asset& min_bp_list_quantity,
		const asset& min_bp_accept_quantity,
		const asset& min_bp_vote_quantity ) {

	require_auth( _self );

	_gstate.max_tally_vote_iterate_steps 	= max_tally_vote_iterate_steps;
	_gstate.max_tally_unvote_iterate_steps 	= max_tally_unvote_iterate_steps;
	_gstate.max_reward_iterate_steps 		= max_reward_iterate_steps;
	_gstate.max_bp_size 					= max_bp_size;
	_gstate.election_round_sec				= election_round_sec;
	_gstate.refund_delay_sec				= refund_delay_sec;
	_gstate.election_round_start_hour		= election_round_start_hour;
	_gstate.min_bp_list_quantity 			= min_bp_list_quantity;
	_gstate.min_bp_accept_quantity 			= min_bp_accept_quantity;
	_gstate.min_bp_vote_quantity			= min_bp_vote_quantity;

}

void mgp_bpvoting::setelect(const uint64_t& last_election_round, const uint64_t& last_execution_round) {
	require_auth( _self );

	_gstate.last_election_round = last_election_round;
	_gstate.last_execution_round = last_execution_round;

}

/**
 *	ACTION: unvote fully or partially
 */
void mgp_bpvoting::unvote(const name& owner, const uint64_t vote_id) {
	require_auth( owner );

	auto ct = current_time_point();

	vote_tbl votes(_self, _self.value);
	auto v_itr = votes.find(vote_id);
	check( v_itr != votes.end(), "vote not found" );
	auto elapsed = ct.sec_since_epoch() - v_itr->voted_at.sec_since_epoch();
	check( elapsed > _gstate.refund_delay_sec, "elapsed " + to_string(elapsed) + "sec, too early to unvote" );
	votes.modify( v_itr, _self, [&]( auto& row ) {
		row.unvoted_at = ct;
	});

	candidate_t candidate(v_itr->candidate);
	check( _dbc.get(candidate), "Err: vote's candidate not found" );
	check( candidate.received_votes >= v_itr->quantity, "Err: candidate received_votes insufficient" );
	candidate.received_votes -= v_itr->quantity;
	_dbc.set(candidate);

	voter_t voter(owner);
	check( _dbc.get(voter), "voter not found" );
	check( voter.total_staked >= v_itr->quantity, "Err: unvote exceeds total staked" );
	voter.total_staked -= v_itr->quantity;
	_dbc.set(voter);

	check( _gstate.total_voted >= v_itr->quantity, "Err: unvote exceeds global total staked" );
	_gstate.total_voted -= v_itr->quantity;

 	{
        token::transfer_action transfer_act{ token_account, { {_self, active_perm} } };
        transfer_act.send( _self, owner, v_itr->quantity, "unvote" );
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
	election_round_t last_round = _gstate.last_execution_round;
	check( _dbc.get(last_round), "Err: last round[" + to_string(_gstate.last_execution_round) + "] not found" );
	check( last_round.next_round_id > 0, "next round not set" );

	auto target_round_id = last_round.next_round_id;
	election_round_t target_round(target_round_id);
	check( _dbc.get(target_round), "Err: target round[ " + to_string(target_round_id) + " ] not found" );
	check( !target_round.execute_completed, "target round[ " + to_string(target_round_id) + " ] already executed" );

	if (!last_round.vote_tally_completed)
		_tally_votes_for_last_round(last_round);

	if (!target_round.unvote_tally_completed)
		_tally_unvotes_for_target_round(target_round);

	if (target_round.unvote_tally_completed && last_round.vote_tally_completed) 
		_reward_through_votes(target_round);
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