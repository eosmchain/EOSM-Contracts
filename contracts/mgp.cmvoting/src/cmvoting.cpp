#include <eosio.token/eosio.token.hpp>
#include <mgp.cmvoting/cmvoting.hpp>

using namespace eosio;
using namespace std;
using std::string;

namespace mgp {

using namespace std;
using namespace eosio;



void mgp_cmvoting::control(const bool& scheme,const bool& vote,const bool& award,const asset& cash_money,const asset& vote_count,const string& close_time){

	_gstate.scheme = scheme;
	_gstate.vote = vote;
	_gstate.award = award;
	_gstate.cash_money = cash_money;
	_gstate.vote_count = vote_count;
	_gstate.close_time = close_time;

}

//添加方案
void mgp_cmvoting::addscheme(const name& account,const string& scheme_title,const string& scheme_content,const asset& cash_money){


	check( _gstate.cash_money.amount == cash_money.amount, "The scheme deposit is inconsistent");

	check( cash_money.amount > 0, "order quanity must be positive" );

	check( cash_money.symbol == SYS_SYMBOL, "Token Symbol not allowed" );

	check(_gstate.scheme,"The delivery scheme has not yet been opened");

	seller_t seller(account);
	check( _dbc.get(seller), "seller not found: " + account.to_string() );
	check( seller.available_quantity >= cash_money, "seller insufficient quantitiy to sell" );
	seller.available_quantity -= cash_money;
	_dbc.set( seller );


	//代表自身
	scheme_tbl scheme(_self, _self.value);
	auto scheme_id = scheme.available_primary_key();
	//插入一条新的
	scheme.emplace( _self, [&]( auto& row ){

		row.id = scheme_id;
		row.account = account;
		row.scheme_title = scheme_title;
		row.scheme_content = scheme_content;
		row.cash_money = cash_money;
		row.created_at = time_point_sec(current_time_point());
		row.updated_at = time_point_sec(current_time_point());
		row.is_del = false;
		row.is_remit = false;
		row.vote_count = asset(0.0000,SYS_SYMBOL);
		row.audit_status = 0;
	});

}

//添加投票记录
void mgp_cmvoting::addvote(const name& account,const asset& vote_count,const uint64_t& scheme_id,const bool& is_super_node,const string& scheme_title,const string& scheme_content){



	check( vote_count.amount > 0, "order quanity must be positive" );
	check( vote_count.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check(_gstate.vote,"The vote has not yet opened");

	if(!is_super_node){

		seller_t seller(account);
		check( _dbc.get(seller), "seller not found: " + account.to_string() );
		check( seller.available_quantity >= vote_count, "seller insufficient quantitiy to sell" );
		seller.available_quantity -= vote_count;
		_dbc.set( seller );

	}
	

	_gstate.vote_count = _gstate.vote_count + vote_count;

	scheme_tbl scheme(_self, _self.value);
	auto itr = scheme.find(scheme_id);
	//判断是否存在
	check(itr != scheme.end(),"Solution does not exist"+ to_string(scheme_id));

	check(itr -> audit_status == 1,"no voting state"+ to_string(scheme_id));
	//更新方案票数
	scheme.modify( *itr, _self, [&]( auto& row ) {
				row.vote_count = row.vote_count + vote_count;
	});

	vote_tbl vote(_self, _self.value);
	auto vote_id = vote.available_primary_key();
	vote.emplace( _self, [&]( auto& row ){
	
		row.id = vote_id;
		row.account = account;
		row.vote_count = vote_count;
		row.is_remit = false;
		row.is_super_node = is_super_node;
		row.is_del = false;
		row.created_at = time_point_sec(current_time_point());
		row.scheme_id = scheme_id;
		row.scheme_title = scheme_title;
		row.scheme_content = scheme_content;
	
	});

	vote_record_tbl voterecord(_self, _self.value);
	bool not_exist = true;
	for(auto itr = voterecord.begin();itr!=voterecord.end(); itr++){

		if(itr -> account == account && scheme_id == itr -> scheme_id){

			not_exist = false;

			voterecord.modify( *itr, _self, [&]( auto& row ) {
		
			row.account = account;
			row.vote_count = row.vote_count + vote_count;
			row.created_at = time_point_sec(current_time_point());
			row.scheme_title = scheme_title;
			row.scheme_content = scheme_content;
			row.scheme_id = scheme_id;

			});

		}

	}
	if(not_exist){
	auto record_id = voterecord.available_primary_key();
		voterecord.emplace( _self, [&]( auto& row ){
			row.id = record_id;
			row.account = account;
			row.vote_count = vote_count;
			row.created_at = time_point_sec(current_time_point());
			row.scheme_title = scheme_title;
			row.scheme_content = scheme_content;
			row.scheme_id = scheme_id;
		});

	}


}

void mgp_cmvoting::award(const asset& super_node_count,const asset& scheme_award,const asset& vote_award){


	check(_gstate.award,"Settlement not opened");


	//投票总数
	asset total_vote = asset(0.0000,SYS_SYMBOL);
	//超级节点投出去的票数
	asset super_node_vote = asset(0.0000,SYS_SYMBOL);
	//普通投出去票数
	asset ordinary_vote_count = asset(0.0000,SYS_SYMBOL);

	//方案通过的id 是否超过方案总数的51%
	uint64_t scheme_suc = -1;
	//普通投票是否超过超级节点总数的51%
	bool super_node_suc = false;



	scheme_tbl scheme(_self, _self.value);
		for(auto itr = scheme.begin();itr!=scheme.end(); itr++){
			if(!(itr->is_del) && !(itr->is_remit)){

				total_vote = total_vote + itr -> vote_count;

				//押金退回
				scheme.modify( *itr, _self, [&]( auto& row ) {
					row.is_remit = true;
				});

				//添加方案押金返回记录
				award_tbl award(_self, _self.value);
				auto award_id = award.available_primary_key();
				award.emplace( _self, [&]( auto& row ){
					row.id = award_id;
					row.account = itr -> account;
					row.created_at = time_point_sec(current_time_point());
					row.money = itr -> cash_money;
					row.award_type = 0;
					row.is_del = false;
				});


				TRANSFER(SYS_BANK,itr -> account,itr -> cash_money,"Return the deposit" );
			}
		}
	
		
	vote_tbl vote(_self, _self.value);
	
		for(auto itr = vote.begin();itr!=vote.end(); itr++){
			if(!(itr -> is_del) && !(itr -> is_super_node) && !(itr -> is_remit)){
				ordinary_vote_count = ordinary_vote_count + itr -> vote_count;
				//投票退回
				vote.modify( *itr, _self, [&]( auto& row ) {
					row.is_remit = true;
				});
				TRANSFER(SYS_BANK,itr->account,itr->vote_count,"Vote back" );
				
				//添加投票返回记录
				award_tbl award(_self, _self.value);
				auto award_id = award.available_primary_key();
				award.emplace( _self, [&]( auto& row ){
					row.id = award_id;
					row.account = itr -> account;
					row.created_at = time_point_sec(current_time_point());
					row.money = itr -> vote_count;
					row.award_type = 1;
					row.is_del = false;
				});

				
			}
			if(!(itr -> is_del) && (itr -> is_super_node) ){
				super_node_vote = super_node_vote + itr -> vote_count;
			}
	}
	if(double(ordinary_vote_count.amount) > double(super_node_count.amount) * 0.51){
		super_node_suc = true;
		scheme_tbl schemesuc(_self, _self.value);
		for(auto itr = schemesuc.begin();itr!=schemesuc.end(); itr++){
			
			if(double(itr -> vote_count.amount) > double(total_vote.amount) * 0.51){
				scheme_suc = itr -> id;
			}

		}
	}


	settle_tbl settle(_self, _self.value);
	auto settle_id = settle.available_primary_key();
	settle.emplace( _self, [&]( auto& row ){
		row.id = settle_id;
		row.scheme_id = scheme_suc;
		row.total_vote = total_vote;
		row.super_node_vote = super_node_vote;
		row.ordinary_vote_count = ordinary_vote_count;
		row.super_node_suc = super_node_suc;
		row.total_super_node = super_node_count;
		row.scheme_award = scheme_award;
		row.vote_award = vote_award;
		row.created_at = time_point_sec(current_time_point());
	});

	//如果符合成功就分发奖励
	if(super_node_suc && scheme_suc != -1){
		scheme_tbl scheme(_self, _self.value);
		auto itr = scheme.find(scheme_suc);

		//添加方案奖励
		award_tbl award(_self, _self.value);
		auto award_id = award.available_primary_key();
		award.emplace( _self, [&]( auto& row ){
			row.id = award_id;
			row.account = itr -> account;
			row.created_at = time_point_sec(current_time_point());
			row.money = scheme_award;
			row.award_type = 2;
			row.is_del = false;
		});

		TRANSFER(SYS_BANK,itr -> account, scheme_award ,"Plan rewards" );

		uint32_t award_vote_count = 0;
		//计算出总票数
		vote_tbl vote(_self, _self.value);
		//投中方案总票数
		for(auto itr = vote.begin();itr!=vote.end(); itr++){

			if(!(itr -> is_del) && scheme_suc == itr -> scheme_id){

				award_vote_count = award_vote_count + itr -> vote_count.amount;
			}

		}

		//奖励
		for(auto itr = vote.begin();itr!=vote.end(); itr++){
			
			if(!(itr -> is_del) && scheme_suc == itr -> scheme_id){
				asset money = asset(double(itr->vote_count.amount) / award_vote_count * vote_award.amount,SYS_SYMBOL);

			//添加投票奖励
			award_tbl award(_self, _self.value);
			auto award_id = award.available_primary_key();
			award.emplace( _self, [&]( auto& row ){
				row.id = award_id;
				row.account = itr -> account;
				row.created_at = time_point_sec(current_time_point());
				row.money = money;
				row.award_type = 3;
				row.is_del = false;
			});
				TRANSFER(SYS_BANK,itr->account,money,"Voted to reward" );
			}
		
		}
	}



}

void mgp_cmvoting::del(){
	require_auth(_self);
	scheme_tbl scheme(_self, _self.value);
	auto s = scheme.begin();
	while(s != scheme.end()){
		s = scheme.erase(s);
	}

	vote_tbl vote(_self, _self.value);
	auto v = vote.begin();
	while(v != vote.end()){
		v = vote.erase(v);
	}

	award_tbl award(_self, _self.value);
	auto a = award.begin();
	while(a != award.end()){
		a = award.erase(a);
	}

	vote_record_tbl record(_self, _self.value);
	auto r = record.begin();
	while(r != record.end()){
		r = record.erase(r);
	}

	settle_tbl settle(_self, _self.value);
	auto sl = settle.begin();
	while(sl != settle.end()){
		sl = settle.erase(sl);
	}
	
	

}

void mgp_cmvoting::deposit(name from, name to, asset quantity, string memo) {
	if (to != _self) return;

	seller_t seller(from);
	_dbc.get( seller );

	if (quantity.symbol == SYS_SYMBOL){
		seller.available_quantity += quantity;
	}

	_dbc.set( seller );

}

void mgp_cmvoting::test(const name& addr,const asset& money){
	TRANSFER(SYS_BANK,addr,money,"测试交易" );
}

void mgp_cmvoting::audit(const uint64_t& id,const uint64_t& audit_status,const string& audit_msg){

	scheme_tbl scheme(_self, _self.value);
	auto itr = scheme.find(id);
	//判断是否存在
	check(itr != scheme.end(),"Solution does not exist"+ to_string(id));
	//修改审核状态
	scheme.modify( *itr, _self, [&]( auto& row ) {
		row.audit_status = audit_status;
		row.updated_at = time_point_sec(current_time_point());
	});

}

void mgp_cmvoting::upscheme(const uint64_t& scheme_id,const name& account,const string& scheme_title,const string& scheme_content){

	scheme_tbl scheme(_self, _self.value);
	auto itr = scheme.find(scheme_id);
	//判断是否存在
	check(itr != scheme.end(),"Solution does not exist"+ to_string(scheme_id));
	check(itr -> audit_status == 2,"Status code error"+to_string(scheme_id));
	//修改审核状态
	scheme.modify( *itr, _self, [&]( auto& row ) {
		row.audit_status = 0;
		row.account = account;
		row.scheme_title = scheme_title;
		row.scheme_content = scheme_content;
		row.updated_at = time_point_sec(current_time_point());
	});

}


}  //end of namespace:: cmvoting
