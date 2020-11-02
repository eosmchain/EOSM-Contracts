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

   
}

[[eosio::action]]
void mgp_bpvoting::deladmins(const std::vector<name>& admins) {
	require_auth( get_self() );

}

[[eosio::action]]
void mgp_bpvoting::propose(const name& issuer, uint64_t proposal_id, const asset& withdraw_quantity){
	require_auth( issuer );

}

[[eosio::action]]
void mgp_bpvoting::approve(const name& issuer, uint64_t proposal_id){
	require_auth( issuer );

   
}

[[eosio::action]]
void mgp_bpvoting::execute(const name& issuer, uint64_t proposal_id){
	require_auth( issuer );

   
}

}  //end of namespace:: mgpbpvoting