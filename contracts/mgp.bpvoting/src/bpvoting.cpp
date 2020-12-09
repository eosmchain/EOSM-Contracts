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
	auto rounds = elapsed / _gstate.election_round_sec + 1;

	return rounds; //usually in days
}

void mgp_bpvoting::_current_election_round(const time_point& ct, election_round_t& curr_round) {
	auto round_id = get_round_id(ct);
	curr_round.round_id = round_id;

	if (!_dbc.get( curr_round )) {
		election_round_t last_election_round(_gstate.last_election_round);
		check( _dbc.get(last_election_round), "Err: last_election_round[" + to_string(_gstate.last_election_round) + "] not found" );
		check( last_election_round.next_round_id == 0, "Err: next_round_id already set" );
		last_election_round.next_round_id = curr_round.round_id;
		_dbc.set( last_election_round );

		//create a new current round
		curr_round.started_at = last_election_round.ended_at;
		auto elapsed = ct.sec_since_epoch() - last_election_round.ended_at.sec_since_epoch();
		auto rounds = elapsed / _gstate.election_round_sec;
		check( rounds > 0 && round_id != 1, "Too early to create a new round" );
		curr_round.ended_at = curr_round.started_at + eosio::seconds(rounds * _gstate.election_round_sec);
		curr_round.created_at = ct;
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
		candidate.tallied_votes				= asset(0, SYS_SYMBOL);
		candidate.last_claimed_rewards 		= asset(0, SYS_SYMBOL);
		candidate.total_claimed_rewards 	= asset(0, SYS_SYMBOL);
		candidate.unclaimed_rewards			= asset(0, SYS_SYMBOL);
	}

	candidate.self_reward_share 			= self_reward_share;
	candidate.staked_votes 					+= quantity;

	_dbc.set( candidate );

}

void mgp_bpvoting::_vote(const name& owner, const name& target, const asset& quantity) {
	time_point ct = current_time_point();
	election_round_t curr_round;
	_current_election_round(ct, curr_round);
	curr_round.vote_count++;
	curr_round.total_votes += quantity;
	_dbc.set( curr_round );

	candidate_t candidate(target);
	check( _dbc.get(candidate), "candidate ("+target.to_string() +") not listed" );
	candidate.received_votes += quantity;
	_dbc.set(candidate);

	vote_tbl votes(_self, _self.value);
	auto vote_id = votes.available_primary_key();
	votes.emplace( _self, [&]( auto& row ) {
		row.id = vote_id;
		row.owner = owner;
		row.candidate = target;
		row.quantity = quantity;
		row.voted_at = ct;
		row.restarted_at = ct;
		row.election_round = curr_round.round_id;
		row.reward_round = curr_round.round_id;
	});

	voteage_t voteage(vote_id, quantity, 0);
	_dbc.set( voteage );

	voter_t voter(owner);
	if (!_dbc.get( voter )) {
		voter.total_staked = asset(0, SYS_SYMBOL);
		voter.last_claimed_rewards = asset(0, SYS_SYMBOL);
		voter.total_claimed_rewards = asset(0, SYS_SYMBOL);
		voter.unclaimed_rewards = asset(0, SYS_SYMBOL);
	}

	voter.total_staked += quantity;
	_dbc.set( voter );

}

void mgp_bpvoting::_elect(election_round_t& round, const candidate_t& candidate) {
	
	round.elected_bps[ candidate.owner ].received_votes = candidate.staked_votes + candidate.tallied_votes;

	typedef std::pair<name, bp_info_t> bp_entry_t;
	// std::vector<bp_info_t, decltype(cmp)> bps(cmp);
	std::vector<bp_entry_t> bps;

	string _bps = "";
	for (auto& it : round.elected_bps) {
		bps.push_back(it);
		_bps += it.first.to_string() + ": " + it.second.received_votes.to_string() + "\n";
	}
	
	auto cmp = [](bp_entry_t a, bp_entry_t b) {return a.second.received_votes > b.second.received_votes; };
	std::sort(bps.begin(), bps.end(), cmp);

	round.elected_bps.clear();
	auto size = (bps.size() == 22) ? 21 : bps.size();
	for (int i = 0; i < size; i++) {
		round.elected_bps.emplace(bps[i].first, bps[i].second);
	}
}

void mgp_bpvoting::_tally_votes(election_round_t& last_round, election_round_t& execution_round) {
	vote_tbl votes(_self, _self.value);
	auto idx = votes.get_index<"electround"_n>();
	auto lower_itr = idx.lower_bound( last_round.round_id );
	auto upper_itr = idx.upper_bound( last_round.round_id );
	int step = 0;

	bool completed = true;
	vector<uint64_t> vote_ids;

	for (auto itr = lower_itr; itr != upper_itr && itr != idx.end(); itr++) {
		if (step++ == _gstate.max_tally_vote_iterate_steps) {
			completed = false;
			break;
		}
	
		vote_ids.push_back(itr->id);

		candidate_t candidate(itr->candidate);
		check( _dbc.get(candidate), "Err: candidate["+ candidate.owner.to_string() +"] not found" );
		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter["+ voter.owner.to_string() +"] not found" );

		candidate.tallied_votes += itr->quantity;
		if (candidate.staked_votes + candidate.tallied_votes >= _gstate.min_bp_accept_quantity)
			_elect(execution_round, candidate);
		
		_dbc.set( candidate );

		voteage_t voteage(itr->id);
		if (!_dbc.get(voteage)) voteage.votes = asset(0, SYS_SYMBOL);
		voteage.age = (voteage.age == 30) ? 1 : voteage.age + 1;
		_dbc.set( voteage );

		execution_round.total_voteage += voteage.value();
		_dbc.set( execution_round );
	}

	for (auto& vote_id : vote_ids) {
		auto itr = votes.find(vote_id);
		check( itr != votes.end(), "Err: vote_id not found: " + to_string(vote_id) );
		votes.modify( *itr, _self, [&]( auto& row ) {
			row.election_round = last_round.next_round_id;
		});
	}
	last_round.vote_tally_completed = completed;
	_dbc.set( last_round );

}

void mgp_bpvoting::_tally_unvotes(election_round_t& round) {
	int step = 0;
	bool completed = true;

	vote_tbl votes(_self, _self.value);
	auto vote_idx = votes.get_index<"unvoteda"_n>();
	auto lower_itr = vote_idx.lower_bound( uint64_t(round.started_at.sec_since_epoch()) );
	auto upper_itr = vote_idx.upper_bound( uint64_t(round.ended_at.sec_since_epoch()) );
	auto itr = lower_itr;
	while (itr != upper_itr && itr != vote_idx.end()) {
		if (step++ == _gstate.max_tally_unvote_iterate_steps) {
			completed = false;
			break;
		}

		candidate_t candidate(itr->candidate);
		check( _dbc.get(candidate), "Err: candidate[ " + candidate.owner.to_string() + "] not found" );
		check( candidate.tallied_votes >= itr->quantity, "Err: unvote exceeded" );
		candidate.tallied_votes -= itr->quantity;
		_elect(round, candidate);

		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		auto new_voteage = false;
		voteage_t voteage(itr->id);
		if (!_dbc.get(voteage)) {
			new_voteage = true;
			voteage.votes = asset(0, SYS_SYMBOL);
		}
		round.total_voteage -= voteage.value();
		round.unvote_count++;

		if (!new_voteage)
			_dbc.del( voteage );

		itr = vote_idx.erase( itr );
	}

	round.unvote_last_round_completed = completed;

	_dbc.set( round  );

}

void mgp_bpvoting::_allocate_rewards(election_round_t& round) {
	if (round.total_received_rewards.amount == 0) {
		round.reward_allocation_completed = true;
		_dbc.set( round );
		return;
	}

	if (round.elected_bps.size() == 0) {
		election_round_t next_round(round.next_round_id);
		check(_dbc.get(next_round), "Err: next round[" + to_string(round.next_round_id) +"] not exist" );
		next_round.total_received_rewards += round.total_received_rewards;
		_dbc.set(next_round);

		round.total_received_rewards.amount = 0;
		round.reward_allocation_completed = true;
		_dbc.set( round );
		return;
	}

	auto per_bp_rewards = (uint64_t) ((double) round.total_received_rewards.amount / round.elected_bps.size());
	// typedef std::pair< name, tuple<asset, asset, asset> > bp_info_t;
	for (auto& item : round.elected_bps) {
		candidate_t bp(item.first);
		check( _dbc.get(bp), "Err: bp (" + bp.owner.to_string() + ") not found" );

		auto bp_rewards = (uint64_t) (per_bp_rewards * (double) bp.self_reward_share / share_boost);
		auto voter_rewards = per_bp_rewards - bp_rewards;
		item.second.allocated_bp_rewards.amount = bp_rewards;
		item.second.allocated_voter_rewards.amount = voter_rewards;
		
		bp.unclaimed_rewards.amount += bp_rewards;
		_dbc.set(bp);		
	}

	round.reward_allocation_completed = true;
	_dbc.set( round );

}

//reward target_round
void mgp_bpvoting::_execute_rewards(election_round_t& round) {
	if (round.elected_bps.size() == 0 || round.total_voteage.amount == 0) {
		_gstate.last_execution_round = round.round_id;
		return;
	}

	vote_tbl votes(_self, _self.value);
	auto idx = votes.get_index<"rewardround"_n>();
	auto upper_itr = idx.upper_bound( round.round_id );

	bool completed = true;
	int step = 0;
	vector<uint64_t> vote_ids;

	for (auto itr = idx.begin(); itr != upper_itr && itr != idx.end(); itr++) {
		if (step++ == _gstate.max_reward_iterate_steps) {
			completed = false;
			break;
		}

		if (!round.elected_bps.count(itr->candidate))
			continue;	//skip vote with its candidate unelected

		vote_ids.push_back(itr->id);

		candidate_t bp(itr->candidate);
		check( _dbc.get(bp), "Err: bp not found" );
		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		voteage_t voteage(itr->id);
		if (!_dbc.get(voteage)) 
			voteage.votes = asset(0, SYS_SYMBOL);

		auto va = voteage.value();
		if (va.amount == 0) continue;

		auto voter_rewards = round.elected_bps[itr->candidate].allocated_voter_rewards * va.amount / round.total_voteage.amount;
		voter.unclaimed_rewards += voter_rewards;

		_dbc.set(voter);

   	}

	for (auto& vote_id : vote_ids) {
		auto itr = votes.find(vote_id);
		check( itr != votes.end(), "Err: vote_id not found: " + to_string(vote_id) );

		votes.modify( *itr, _self, [&]( auto& row ) {
      		row.reward_round = round.next_round_id;
   		});
	}

	if (completed) {
		_gstate.last_execution_round = round.round_id;
	}


	_dbc.set( round );

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
	auto ct = current_time_point();
	reward_t reward( _self, _self.value );
	reward.quantity = quantity;
	reward.created_at = ct;
	_dbc.set( reward );

	election_round_t curr_round;
	_current_election_round( ct, curr_round );
	curr_round.total_received_rewards += quantity;
	_dbc.set( curr_round );

	_gstate.total_received_rewards += quantity;

}

/*************** Begin of ACTION functions ***************************************/

/**
 *	ACTION: kick start the election
 */
void mgp_bpvoting::init() {
	require_auth( _self );

	check (_gstate.started_at == time_point(), "already kickstarted" );

	auto ct = current_time_point();
	_gstate.started_at 						= ct;
	_gstate.last_election_round 			= 1;
	_gstate.last_execution_round 			= 0;

	auto days 								= ct.sec_since_epoch() / seconds_per_day;
	auto start_secs 						= _gstate.election_round_start_hour * 3600;

	election_round_t election_round(1);
	election_round.started_at 				= time_point() + eosio::seconds(days * seconds_per_day + start_secs);
	election_round.ended_at 				= election_round.started_at + eosio::seconds(_gstate.election_round_sec);
	election_round.created_at 				= ct;
	election_round.vote_tally_completed 	= false;
	election_round.unvote_last_round_completed 	= true;
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

void mgp_bpvoting::setelect(const uint64_t& election_round, const uint64_t& execution_round) {
	require_auth( _self );

	_gstate.last_election_round = election_round;
	_gstate.last_execution_round = execution_round;

}

void mgp_bpvoting::syncvoteages() {
	voteage_t voteages;
	auto tbl = _dbc.get_tbl(voteages);
	for (auto itr = tbl.begin(); itr != tbl.end(); itr++) {
		tbl.modify( *itr, _self, [&]( auto& row ) {
			vote_t vote(itr->vote_id);
			check(_dbc.get(vote), "Err, vote[" + to_string(itr->vote_id) + "] not exist");

			row.votes = vote.quantity;
			auto ct = current_time_point();
			auto elapsed = ct.sec_since_epoch() - vote.voted_at.sec_since_epoch();
			auto days = elapsed / seconds_per_day;
			row.age = days;
		});
	}
}

/**
 *	ACTION: unvote fully or partially
 */
void mgp_bpvoting::unvote(const name& owner, const uint64_t vote_id) {
	require_auth( owner );

	auto ct = current_time_point();
	check( get_round_id(ct) > 0, "unvote not allowed in round-0" );

	vote_tbl votes(_self, _self.value);
	auto v_itr = votes.find(vote_id);
	check( v_itr != votes.end(), "vote not found" );
	auto elapsed = ct.sec_since_epoch() - v_itr->voted_at.sec_since_epoch();
	check( elapsed > _gstate.refund_delay_sec, "elapsed " + to_string(elapsed) + "sec, too early to unvote" );
	votes.modify( v_itr, _self, [&]( auto& row ) {
		row.unvoted_at = ct;
	});

	candidate_t candidate(v_itr->candidate);
	check( _dbc.get(candidate), "Err: vote's candidate not found: " + candidate.owner.to_string() );
	check( candidate.received_votes >= v_itr->quantity, "Err: candidate received_votes insufficient: " + to_string(v_itr->quantity) );
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

	election_round_t last_execution_round(_gstate.last_execution_round);
	if (last_execution_round.round_id == 0) { //virtual round (non-existent)
		last_execution_round.next_round_id = 1;
		last_execution_round.vote_tally_completed = true;
		last_execution_round.reward_allocation_completed = true;
	} else
		_dbc.get(last_execution_round);

	election_round_t execution_round(last_execution_round.next_round_id);
	check( _dbc.get(execution_round), "Err: execution_round[" + to_string(execution_round.round_id) + "] not exist" );
	check( execution_round.next_round_id > 0, "execution_round[" + to_string(execution_round.round_id) + "] not ended yet" );

	if (!last_execution_round.vote_tally_completed && last_execution_round.round_id > 0)
	{
		if (execution_round.round_id > 1 && execution_round.elected_bps.size() == 0) //copy for the first time
			execution_round.elected_bps = last_execution_round.elected_bps;

		_tally_votes( last_execution_round, execution_round );
	}

	if (last_execution_round.vote_tally_completed && !execution_round.unvote_last_round_completed) 
	{
		_tally_unvotes( execution_round );
	}

	if (last_execution_round.vote_tally_completed && 
		execution_round.unvote_last_round_completed && 
		!execution_round.reward_allocation_completed ) 
	{
		_allocate_rewards( execution_round );
	}

	if (last_execution_round.vote_tally_completed && 
		execution_round.reward_allocation_completed &&
		execution_round.unvote_last_round_completed) 
	{
		_execute_rewards( execution_round );
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