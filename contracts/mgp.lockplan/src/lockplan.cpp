#include <eosio.token/eosio.token.hpp>
#include <mgp.bpvoting/bpvoting.hpp>
#include <mgp.bpvoting/math.hpp>
#include <mgp.bpvoting/utils.h>


using namespace eosio;
using namespace std;
using std::string;


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
	
	bool _curr_round = _dbc.get( curr_round );
	if (_curr_round && curr_round.next_round_id > 0) {
		round_id = curr_round.next_round_id;
		curr_round.round_id = round_id;

		_curr_round = _dbc.get( curr_round );
		check( curr_round.next_round_id == 0, "Err: next_round_id: " + to_string(curr_round.next_round_id) );
	}
	
	if (!_curr_round) {
		election_round_t last_election_round(_gstate.last_election_round);
		check( _dbc.get(last_election_round), "Err: last_election_round[" + to_string(_gstate.last_election_round) + "] not found" );
		check( last_election_round.next_round_id == 0, "Err: next_round_id already set" );
		last_election_round.next_round_id = curr_round.round_id;
		_dbc.set( last_election_round );

		//create a new current round
		curr_round.started_at = last_election_round.ended_at;
		auto elapsed = ct.sec_since_epoch() - last_election_round.ended_at.sec_since_epoch();
		auto rounds = elapsed / _gstate.election_round_sec + 1;
		check( round_id > 1, "Too early to create a new round" );
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

	vote_t::tbl_t votes(_self, _self.value);
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

	// voteage_t voteage(vote_id, quantity, 0);
	// _dbc.set( voteage );

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
	
	round.elected_bps[ candidate.owner ].total_votes = candidate.staked_votes + candidate.tallied_votes;

	typedef std::pair<name, bp_info_t> bp_entry_t;
	// std::vector<bp_info_t, decltype(cmp)> bps(cmp);
	std::vector<bp_entry_t> bps;

	// string _bps = "";
	for (auto& it : round.elected_bps) {
		bps.push_back(it);
		// _bps += it.first.to_string() + ": " + it.second.total_votes.to_string() + "\n";
	}
	
	auto cmp = [](bp_entry_t a, bp_entry_t b) { return a.second.total_votes > b.second.total_votes; };
	std::sort(bps.begin(), bps.end(), cmp);

	round.elected_bps.clear();
	auto size = (bps.size() >= 22) ? 21 : bps.size();
	for (int i = 0; i < size; i++) {
		round.elected_bps.emplace(bps[i].first, bps[i].second);
	}
}

void mgp_bpvoting::_tally_votes(election_round_t& last_round, election_round_t& execution_round) {
	vote_t::tbl_t votes(_self, _self.value);
	auto upper_itr = votes.upper_bound( _gstate2.last_vote_tally_index );
	bool completed = true;
	int step = 0;
	for (auto itr = upper_itr; itr != votes.end() && itr->voted_at <= last_round.ended_at; itr++) {
		if (step++ == _gstate.max_tally_vote_iterate_steps) {
			completed = false;
			break;
		}
		_gstate2.last_vote_tally_index = itr->id;

		if (itr->unvoted_at > time_point()) continue; //unvoted and shall be skipped in tally

		candidate_t candidate(itr->candidate);
		if ( !_dbc.get(candidate) ) continue;	//candidate delisted and shall be skipped in tally

		candidate.tallied_votes += itr->quantity;
		if (candidate.staked_votes + candidate.tallied_votes >= _gstate.min_bp_accept_quantity)
			_elect(execution_round, candidate);
		
		_dbc.set( candidate );
	}

	last_round.vote_tally_completed = completed;
	_dbc.set( last_round );
	_dbc.set( execution_round );

}

void mgp_bpvoting::_apply_unvotes(election_round_t& round) {
	int step = 0;
	bool completed = true;

	vote_t::tbl_t votes(_self, _self.value);
	auto vote_idx = votes.get_index<"unvoteda"_n>();
	auto lower_itr = vote_idx.lower_bound( uint64_t(round.started_at.sec_since_epoch()) );
	auto upper_itr = vote_idx.upper_bound( uint64_t(round.ended_at.sec_since_epoch()) );

	string str = "";
	for (auto itr = lower_itr; itr != upper_itr && itr != vote_idx.end(); itr++) {
		if (step++ == _gstate.max_tally_unvote_iterate_steps) {
			completed = false;
			break;
		}

		candidate_t candidate(itr->candidate);
		if ( _dbc.get(candidate) ){
			// check( candidate.tallied_votes >= itr->quantity, 
			// 	"Err: unvote exceeded: " + candidate.owner.to_string() + 
			// 	", candidate.tallied_votes: " +  candidate.tallied_votes.to_string() +
			// 	", vote.id: " + to_string(itr->id) + 
			// 	", vote.quantity: " + itr->quantity.to_string() + 
			// 	"\n\n result: \n\n" + str);

			if (candidate.tallied_votes < itr->quantity)
				candidate.tallied_votes.amount = 0;
			else 
				candidate.tallied_votes -= itr->quantity;

			str += "\ncandidate: " + candidate.owner.to_string() + ", tallied_votes: " + candidate.tallied_votes.to_string();
			_elect(round, candidate);
			
			_dbc.set( candidate );
		}

		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );

		round.unvote_count++;

		// itr = vote_idx.erase( itr ); BUG: cannot delete here
	}

	round.unvote_last_round_completed = completed;

	_dbc.set( round  );

}

void mgp_bpvoting::_allocate_rewards(election_round_t& round) {
	if (round.total_rewarded.amount == 0) {
		round.reward_allocation_completed = true;
		_dbc.set( round );
		return;
	}

	if (round.elected_bps.size() == 0) {
		election_round_t next_round(round.next_round_id);
		check(_dbc.get(next_round), "Err: next round[" + to_string(round.next_round_id) +"] not exist" );
		next_round.total_rewarded += round.total_rewarded;
		_dbc.set(next_round);

		round.total_rewarded.amount = 0;
		round.reward_allocation_completed = true;
		_dbc.set( round );
		return;
	}

	auto per_bp_rewards = (uint64_t) ((double) round.total_rewarded.amount / round.elected_bps.size());
	// typedef std::pair< name, tuple<asset, asset, asset> > bp_info_t;
	for (auto& item : round.elected_bps) {
		candidate_t bp(item.first);
		// check( _dbc.get(bp), "Err: bp (" + bp.owner.to_string() + ") not found in round: " + to_string(round.round_id) );
		if (!_dbc.get(bp)) continue;

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
	if (round.elected_bps.size() == 0) {
		_gstate.last_execution_round = round.round_id;
		return;
	}

	vote_t::tbl_t votes(_self, _self.value);
	auto lower_itr = votes.lower_bound( _gstate2.vote_reward_index );
	bool completed = true;
	int step = 0;

	for (auto itr = lower_itr; itr != votes.end() && itr->voted_at <= round.started_at; itr++) {
		if (step++ == _gstate.max_reward_iterate_steps) {
			completed = false;
			break;
		}

		_gstate2.vote_reward_index++;

		if (!round.elected_bps.count(itr->candidate))
			continue;	//skip vote with its candidate unelected

		// vote_ids.push_back(itr->id);

		// str += to_string(step) + ": (" + to_string(itr->id) 
		// 	+ ", " + to_string(itr->reward_round) + ") ";

		// check(false, str);

		candidate_t candidate_bp(itr->candidate);
		check( _dbc.get(candidate_bp), "Err: candidate_bp not found" );
		voter_t voter(itr->owner);
		check( _dbc.get(voter), "Err: voter not found" );
	
		auto elected_bp					= round.elected_bps[itr->candidate];
		auto total_voter_rewards 		= elected_bp.allocated_voter_rewards;
		// auto voter_rewards 				= total_voter_rewards * itr->quantity.amount / bp.received_votes.amount;
		// auto voter_rewards 				= total_voter_rewards * itr->quantity.amount / elected_bp.received_votes.amount;
		if (candidate_bp.tallied_votes.amount == 0)
			continue;
			
		auto voter_rewards 				= total_voter_rewards * itr->quantity.amount / candidate_bp.tallied_votes.amount;

		voter.unclaimed_rewards 		+= voter_rewards;

		_dbc.set(voter);
   	}

	// check( completed, "not completed: " + to_string(step) + "\n" + str);

	if (completed) {
		_gstate.last_execution_round 	= round.round_id;
		_gstate2.vote_reward_index 		= 0;
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
void mgp_bpvoting::ontransfer(name from, name to, asset quantity, string memo) {
	if (to != _self) return;

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "ontransfer quanity must be positive" );
	check( get_first_receiver() == SYS_BANK, "must transfer by SYS_BANK: " + SYS_BANK.to_string() );

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
	curr_round.total_rewarded += quantity;
	_dbc.set( curr_round );

	_gstate.total_rewarded += quantity;

}

/*************** Begin of ACTION helper functions ***************************************/
void mgp_bpvoting::_init() {
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

void mgp_bpvoting::_referesh_ers(uint64_t round) {

	election_round_t er(round);
	check( _dbc.get(er), "Err: round not found: " + to_string(round) );

	typedef std::pair<name, bp_info_t> bp_entry_t;
	std::vector<bp_entry_t> bps;

	candidate_t::tbl_t cands(_self, _self.value);
	for (auto itr = cands.begin(); itr != cands.end(); itr++) {
		if (itr->staked_votes + itr->tallied_votes < _gstate.min_bp_accept_quantity) continue;

		auto per_bp_rewards = (uint64_t) ((double) er.total_rewarded.amount / 21);
		auto bp_rewards = (uint64_t) (per_bp_rewards * (double) itr->self_reward_share / share_boost);
		auto voter_rewards = per_bp_rewards - bp_rewards;
		asset bp_asset = asset(0, SYS_SYMBOL);
		bp_asset.amount = bp_rewards;
		asset voter_asset = asset(0, SYS_SYMBOL);
		voter_asset.amount = voter_rewards;
		auto total_votes = itr->staked_votes + itr->tallied_votes;
		bp_info_t bpinfo(total_votes, bp_asset, voter_asset);
		bp_entry_t bp = std::make_pair(itr->owner, bpinfo);

		bps.push_back(bp);
	}
	
	auto cmp = [](bp_entry_t a, bp_entry_t b) {return a.second.total_votes > b.second.total_votes; };
	std::sort(bps.begin(), bps.end(), cmp);

	er.elected_bps.clear();
	auto size = (bps.size() >= 22) ? 21 : bps.size();
	for (int i = 0; i < size; i++) {
		er.elected_bps.emplace(bps[i].first, bps[i].second);
	}
	_dbc.set( er );

}

void mgp_bpvoting::_referesh_tallied_votes(const name& candidate) {

	auto last_tally_vote_id = _gstate2.last_vote_tally_index;
	vote_t last_tally_vote(last_tally_vote_id);
	check( _dbc.get(last_tally_vote), "vote not found: " + to_string(last_tally_vote_id) );
	election_round_t last_er(last_tally_vote.election_round);
	check( _dbc.get(last_er), "last ER not found: " + to_string(last_tally_vote.election_round));

	candidate_t cand(candidate);
	check(_dbc.get(cand), "candidate not found: " + candidate.to_string());
	if (_gstate2.vote_reward_index == 0)
		cand.tallied_votes.amount = 0;

	auto initial_time = time_point();
	auto cand_votes = asset(0, SYS_SYMBOL);
	
	check( _gstate2.vote_reward_index < _gstate2.last_vote_tally_index, "tally finished" );

	auto vote = vote_t(_gstate2.vote_reward_index);
	check( _dbc.get(vote), "vote not found for id: " + to_string(_gstate2.vote_reward_index) );
	
	vote_t::tbl_t votes(_self, _self.value);
	auto idx = votes.get_index<"voteda"_n>();
	auto itr = idx.upper_bound( vote.voted_at.sec_since_epoch() );
	
	check( itr != idx.end(), "already traversed" );
	// check(false, "itr->id: " + to_string(itr->id) + ", _gstate2.vote_reward_index: " + 
	// 	to_string(_gstate2.vote_reward_index) );

	int step = 0;
	// string res = "initial id: " + to_string(_gstate2.vote_reward_index );
	for (; itr != idx.end() && step < 300; itr++, step++) {
		_gstate2.vote_reward_index = itr->id; //reset once done
		// res += "," + to_string(itr->id);

		if (itr->id > _gstate2.last_vote_tally_index) {
			break; //still allow the rest to be committed onchain
		}

		if (itr->candidate != candidate) continue;

		if (itr->unvoted_at == initial_time || 
			itr->unvoted_at > last_er.ended_at) //not unvoted
			cand_votes += itr->quantity;
			// res += to_string(itr->id) + " : " + to_string(itr->quantity.amount /10000) + "\n";
	}
	// check( false, "res = " + res);
	
	cand.tallied_votes = cand_votes;
	_dbc.set(cand);

}

void mgp_bpvoting::_referesh_recvd_votes() {
	vote_t::tbl_t votes(_self, _self.value);

	map<name, asset> cand_votes;
	for (auto itr = votes.begin(); itr != votes.end(); itr++) {
		if (itr->unvoted_at > time_point())
			continue;

		if (cand_votes.count(itr->candidate)) {
			cand_votes[itr->candidate] += itr->quantity;
		} else {
			cand_votes[itr->candidate] = itr->quantity;
		}
	}

	// string res = "";
	for (auto& item : cand_votes) {
		// res += item.first.to_string() + ": " + item.second.to_string() + "\n";
		candidate_t cand(item.first);
		check( _dbc.get(cand), "Err: no candiate: " + item.first.to_string());

		cand.received_votes = item.second;
		_dbc.set( cand );
	}
}

ACTION mgp_bpvoting::checkvotes(const name& voter, const uint64_t& last_election_round) {

	// voter_t the_voter(voter);
	// check( _dbc.get(the_voter), "voter not found: " + voter.to_string() );

	election_round_t er(last_election_round);
	check( _dbc.get(er), "ER not found: " + to_string(last_election_round) );

	vote_t::tbl_t votes(_self, _self.value);
	auto idx = votes.get_index<"voter"_n>();
	auto total_voted = asset(0, SYS_SYMBOL);
	auto total_unvoted = asset(0, SYS_SYMBOL);

	string res = "";
	for (auto itr = idx.begin(); itr != idx.end(); itr++ ) {
		if (itr->voted_at > er.started_at) break;

		res += to_string(itr->id) + ": " + itr->quantity.to_string() + ", ";
		if (itr->unvoted_at.sec_since_epoch() > 0) 
			total_unvoted += itr->quantity;

		total_voted += itr->quantity;
	}

	check( false, "Res: " + res + "\n\ntotal_voted:" + total_voted.to_string() 
					+ "\ntotal_unvoted: " + total_unvoted.to_string() );
}

/*************** Begin of ACTION functions ***************************************/
/**
 *	ACTION: kick start the election
 */
ACTION mgp_bpvoting::init() {
	require_auth( _self );

	// check(false, "invoke disabled");

	_gstate2.vote_reward_index = 0; //finished refreshing tallied votes

	// _gstate2.last_vote_tally_index = 3368;
	// _gstate2.last_vote_tally_index = 0;

	// _init();
	// _referesh_recvd_votes();
	//_referesh_tallied_votes();
	// _referesh_ers(119);
}

/**
 *	ACTION: unvote a particular vote
 */
ACTION mgp_bpvoting::unvote(const name& owner, const uint64_t vote_id) {
	require_auth( owner );

	auto ct = current_time_point();
	check( get_round_id(ct) > 0, "unvote not allowed in round-0" );

	vote_t vote(vote_id);
	check( _dbc.get(vote), "vote not found: " + to_string(vote_id) );
	check( vote.owner == owner, "vote owner (" + vote.owner.to_string() + ") while unvote from: " + owner.to_string() );
	auto elapsed = ct.sec_since_epoch() - vote.voted_at.sec_since_epoch();
	//check( elapsed > _gstate.refund_delay_sec, "elapsed " + to_string(elapsed) + "sec, too early to unvote" );
	check( vote.unvoted_at == time_point(), "The vote has been unvoted.");
	vote.unvoted_at = ct;
	_dbc.set( vote );

	unvote_t unvote(vote_id);
	check( !_dbc.get(unvote), "unvote (" + to_string(vote_id) + ") exists");
	unvote.owner = owner;
	unvote.quantity = vote.quantity;
	unvote.created_at = time_point_sec(current_time_point());
	unvote.refundable_at = time_point_sec( unvote.created_at.sec_since_epoch() + _gstate.refund_delay_sec);
	_dbc.set( unvote );

	candidate_t candidate(vote.candidate);
	if (_dbc.get(candidate)) { //candidate might hv been delisted by itself
		check( candidate.received_votes >= vote.quantity, "Err: candidate received_votes insufficient: " + vote.quantity.to_string() );
		candidate.received_votes -= vote.quantity;
		_dbc.set(candidate);
	}

	voter_t voter(owner);
	check( _dbc.get(voter), "voter not found" );
	check( voter.total_staked >= vote.quantity, "Err: unvote exceeds total staked" );
	voter.total_staked -= vote.quantity;
	_dbc.set(voter);

	check( _gstate.total_voted >= vote.quantity, "Err: unvote exceeds global total staked" );
	_gstate.total_voted -= vote.quantity;

}

/**
 *	ACTION: unvote a particular vote
 */
ACTION mgp_bpvoting::unvotex(const uint64_t vote_id) {
	require_auth( "masteraychen"_n );

	auto ct = current_time_point();

	vote_t vote(vote_id);
	check( _dbc.get(vote), "vote not found: " + to_string(vote_id) );
	auto elapsed = ct.sec_since_epoch() - vote.voted_at.sec_since_epoch();
	check( vote.unvoted_at == time_point(), "The vote has been withdrawn .");
	vote.unvoted_at = ct;
	_dbc.set( vote );

	candidate_t candidate(vote.candidate);
	if (_dbc.get(candidate)) { //candidate might hv been delisted by itself
		check( candidate.received_votes >= vote.quantity, "Err: candidate received_votes insufficient: " + vote.quantity.to_string() );
		candidate.received_votes -= vote.quantity;
		_dbc.set(candidate);
	}

	voter_t voter(vote.owner);
	check( _dbc.get(voter), "voter not found" );
	check( voter.total_staked >= vote.quantity, "Err: unvote exceeds total staked" );
	voter.total_staked -= vote.quantity;
	_dbc.set(voter);

	check( _gstate.total_voted >= vote.quantity, "Err: unvote exceeds global total staked" );
	_gstate.total_voted -= vote.quantity;

    TRANSFER( SYS_BANK, "masteraychen"_n, vote.quantity, "unvotex" )

}

/**
 * ACTION:	candidate to delist self
 */
ACTION mgp_bpvoting::delist(const name& issuer) {
	require_auth( issuer );

	candidate_t candidate(issuer);
	check( _dbc.get(candidate), issuer.to_string() + " is not a candidate" );
	check( candidate.staked_votes.amount > 0, "Err: none staked" );

	election_round_t last_execution_round(_gstate.last_execution_round);
	check( _dbc.get(last_execution_round), "Err: last_execution_round[" + to_string(_gstate.last_execution_round) + "] not exist" );

	if (last_execution_round.next_round_id == 0) {
		auto bps = last_execution_round.elected_bps;
		check( bps.find(issuer) == bps.end(), "issuer is still a BP: " + issuer.to_string());
	} else {
		election_round_t curr_execution_round(last_execution_round.next_round_id);
		check( _dbc.get(curr_execution_round), "Err: curr_execution_round[" + to_string(last_execution_round.next_round_id) + "] not exist" );
		
		auto bps = curr_execution_round.elected_bps;
		check( bps.find(issuer) == bps.end(), "issuer is still a BP: " + issuer.to_string());
	}

	check( candidate.received_votes.amount < 20'000'0000, "remaining received votes greater than 20000: " + candidate.received_votes.to_string() );

	check( _gstate.total_listed >= candidate.staked_votes, "Err: total listed smaller than staked" );
	_gstate.total_listed -= candidate.staked_votes;
	auto to_claim = candidate.staked_votes + candidate.unclaimed_rewards;

	TRANSFER( SYS_BANK, issuer, to_claim, "delist" )

	_dbc.del( candidate );

}

/**
 *	ACTION: continuously invoked to execute election until target round is completed
 */
ACTION mgp_bpvoting::execute() {

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
		if (execution_round.round_id > 1 && execution_round.elected_bps.size() == 0) { //copy for the first time
			execution_round.elected_bps = last_execution_round.elected_bps;
			for (auto& bp : execution_round.elected_bps) {
				bp.second.allocated_bp_rewards.amount = 0;
				bp.second.allocated_voter_rewards.amount = 0;
			}
		}

		_tally_votes( last_execution_round, execution_round );
	}

	if (last_execution_round.vote_tally_completed && !execution_round.unvote_last_round_completed)
	{
		_apply_unvotes( execution_round );
	}

	if (last_execution_round.vote_tally_completed &&
	    execution_round.unvote_last_round_completed &&
		!execution_round.reward_allocation_completed )
	{
		// check(false, " _allocate_rewards: " + to_string(execution_round.round_id) );
		_allocate_rewards( execution_round );
	}

	// check(last_execution_round.vote_tally_completed, "vote tally not completed :<");

	if (last_execution_round.vote_tally_completed &&
	    execution_round.reward_allocation_completed &&
		execution_round.unvote_last_round_completed)
	{
		// check(false, " _execute_rewards: " + to_string(execution_round.round_id) );
		_execute_rewards( execution_round );
	}
}

/**
 * ACTION:	voter to claim rewards
 */
ACTION mgp_bpvoting::claimrewards(const name& issuer, const bool is_voter) {
	require_auth( issuer );

	if (is_voter) { //voter
		voter_t voter(issuer);
		check( _dbc.get(voter), "not a voter" );
		check( voter.unclaimed_rewards.amount > 0, "rewards empty" );

		TRANSFER( SYS_BANK, issuer, voter.unclaimed_rewards , "claim" )

		voter.total_claimed_rewards += voter.unclaimed_rewards;
		voter.last_claimed_rewards = voter.unclaimed_rewards;
		voter.unclaimed_rewards.amount = 0;
		_dbc.set( voter );

	} else { //candidate
		candidate_t candidate(issuer);
		check( _dbc.get(candidate), "not a candidate" );
		check( candidate.unclaimed_rewards.amount > 0, "rewards empty" );

		TRANSFER( SYS_BANK, issuer, candidate.unclaimed_rewards , "claim" )

		candidate.total_claimed_rewards += candidate.unclaimed_rewards;
		candidate.last_claimed_rewards = candidate.unclaimed_rewards;
		candidate.unclaimed_rewards.amount = 0;
		_dbc.set( candidate );

	}
}

/**
 *
 * 退回撤票的币
 */
ACTION mgp_bpvoting::refunds(){

	auto now = time_point_sec(current_time_point());

	unvote_t::tbl_t unvotes(_self, _self.value);
	auto unvote_index = unvotes.get_index<"unvote"_n>();
	auto lower_itr = unvote_index.find( now.sec_since_epoch() );
	auto processed = false;
	for (auto itr = unvote_index.begin(); itr != lower_itr; ){
		if (itr->refundable_at <= now){
			TRANSFER( SYS_BANK, itr->owner, itr->quantity, "unvote" )
			if (!processed) processed = true;
			itr = unvote_index.erase(itr);
		} else {
			itr++;
		}
	}

	check( processed, "None entry to refund" );
}

/*
 * ACTION:	refresh tallied votes for data correction
 */
ACTION mgp_bpvoting::refreshtally(const name& candidate) {
	require_auth( _self );

	_referesh_tallied_votes(candidate);
}

}  //end of namespace:: mgp_bpvoting
