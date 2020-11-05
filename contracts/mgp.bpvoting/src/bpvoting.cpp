#include <mgp.bpvoting/bpvoting.hpp>
#include <eosio.token/eosio.token.hpp>

using namespace eosio;
using namespace std;
using std::string;

//account: mgp.bpvoting
namespace mgp {

using namespace eosio;

void mgp_bpvoting::withdraw(const asset& quantity) {
	require_auth( get_self() );

   
}


//memo: {$cmd}:{$target}, Eg: "list:mgpbpooooo11", "vote:mgpbpooooo11"
void mgp_bpvoting::transfer(name from, name to, asset quantity, string memo) {
	require_auth( from );
	if (to != _self) return;

    std::vector<string> transfer_memo = string_split(memo, ':');
    check( transfer_memo.size() == 2, "params error" );
	string cmd = transfer_memo[0];
	string target = transfer_memo[1];

	if (cmd == "list") {
		check( quantity.amount >= _gstate.min_bp_list_amount, "min bp list amount not reached" );

		process_list(quantity, target);

	} else if (cmd == "vote") {
		process_vote(quantity, target);

	} else { //rewards
		// do nothing but take the rewards
	}
}

void process_list(const asset& quantity, const name& target) {

}

void process_vote(const asset& quantity, const name& target) {
	
}

}  //end of namespace:: mgpbpvoting