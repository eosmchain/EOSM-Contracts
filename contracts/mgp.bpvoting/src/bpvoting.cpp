#include <mgp.bpvoting/bpvoting.hpp>
#include <eosio.token/eosio.token.hpp>

using namespace eosio;
using namespace std;
using std::string;

//account: mgp.bpvoting
namespace mgp {

using namespace eosio;

[[eosio::action]]
void mgp_bpvoting::addadmins(const std::vector<name>& admins) {
	require_auth( get_self() );

    auto new_total = _gstate.bpvoting_members.size() + admins.size();
    check( new_total <= 11, "admins oversized: " + to_string(new_total) );

    for (int i = 0; i < admins.size(); i++) {
        _gstate.bpvoting_members.insert(admins[i]);
    }
}

[[eosio::action]]
void mgp_bpvoting::deladmins(const std::vector<name>& admins) {
	require_auth( get_self() );

    for (int i = 0; i < admins.size(); i++) {
        _gstate.bpvoting_members.erase(admins[i]);
    }
}

[[eosio::action]]
void mgp_bpvoting::propose(const name& issuer, uint64_t proposal_id, const asset& withdraw_quantity){
	require_auth( issuer );

    auto id = gen_new_id(P_COUNTER);
    check( id == proposal_id, "proposal_id != " + to_string(id) );

    asset self_bal = eosio::token::get_balance(SYS_BANK, get_self(), SYS_SYMBOL.code());
    check(withdraw_quantity <= self_bal, "proposed overdrawn: self=" + to_string(self_bal.amount) 
                                        + " vs withdraw=" + to_string(withdraw_quantity.amount) );

    auto ct = current_time_point() + hours(_gstate.proposal_expire_in_hours);
    proposal_t proposal(id, issuer, withdraw_quantity, ct);
    _dbc.set(proposal);
}

[[eosio::action]]
void mgp_bpvoting::approve(const name& issuer, uint64_t proposal_id){
	require_auth( issuer );

    proposal_t proposal(proposal_id);
    check( _dbc.get(proposal), "proposal [" + to_string(proposal_id) + "] not exist" );
    check( proposal.expired_at >= current_time_point(), "proposal expired" );
    check( _gstate.bpvoting_members.find(issuer) != _gstate.bpvoting_members.end(), 
            issuer.to_string() + " not a bpvoting member");
    check( proposal.approval_members.find(issuer) != proposal.approval_members.end(), 
            issuer.to_string() + " already approved");

    proposal.approval_members.insert(issuer);
    _dbc.set(proposal);
}

[[eosio::action]]
void mgp_bpvoting::execute(const name& issuer, uint64_t proposal_id){
	require_auth( issuer );

    proposal_t proposal(proposal_id);
    check( _dbc.get(proposal), "proposal [" + to_string(proposal_id) + "] not exist" );
    check( proposal.approval_members.size() >= _gstate.min_approval_size, "not enough approvals" );

    auto member_quant = proposal.propose_to_withdraw / _gstate.bpvoting_members.size();

    for (auto& member : _gstate.bpvoting_members) {
        action(
			permission_level{ get_self(), "active"_n },
			SYS_BANK, "transfer"_n,
			std::make_tuple( get_self(), member, member_quant, "")
		).send();
    }

}

}  //end of namespace:: mgpbpvoting