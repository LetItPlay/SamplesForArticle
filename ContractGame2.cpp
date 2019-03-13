#include <eosiolib/eosio.hpp>
#include <eosiolib/system.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.h>

const uint64_t expected_value = 99; // casino earns 1%

using namespace eosio;
using namespace std;

CONTRACT CGame2 : public contract {
private:
	uint64_t get_rand() {
		return (now() + tapos_block_prefix()) * 179424691u % 0x0fedcba7afffffff; //some magic values
	}

	void dice_game(asset quantity, name player, string memo)
	{
		string delimiter = ";";
		string usr_roll_under = memo.substr(0, memo.find(delimiter));
		bool is_num = usr_roll_under.find_first_not_of("0123456789") == std::string::npos;
		eosio_assert(is_num, "specify roll_under in memo!");
		uint64_t roll_under = stoull(usr_roll_under);
		eosio_assert((roll_under <= 96) && (roll_under >= 3), "roll_under must be in [3..96]!");
		print(" roll_under is ", roll_under);
		auto roll = get_rand() % 100 + 1;
		print(" roll is ", roll);
		if (roll < roll_under)
		{
			uint64_t coef = expected_value * 10000 / (roll_under - 1);
			auto win = asset((quantity.amount * coef) / 10000, quantity.symbol);
			print(" win is ", win.amount);
			pay_to_user(player, win, static_cast<uint8_t>(roll));
		}
		else
			print(" lose!");
	}

	void pay_to_user(name player, asset amount, uint8_t roll)
	{
		action(
		permission_level{ get_self(), name("active") },
		name("eosio.token"), name("transfer"),
		std::make_tuple(
			get_self(),
			player,
			amount,
			string("You win in dice game! Roll : ") + std::to_string(roll)
		)
		).send();
	}
public:
	using contract::contract;
	CGame2(name receiver, name code, datastream<const char*> ds)
		: contract(receiver, code, ds)
	{};

	void apply_transfer(name from, name to, asset quantity, string memo)
	{
		if (from == get_self()) // we are transfering from us
			return;
		if (to != get_self())
		{
			print("you shall not pass!");
			return; // don't allow cross calls!
		}
		print("from : ", from, " to : ", to, ", quantity : ", quantity.amount, ", memo : ", memo);
		dice_game(quantity, from, memo);
	}
};

//EOSIO_DISPATCH(CGame2, (transfer))

extern "C" {
	void apply(uint64_t receiver, uint64_t code, uint64_t action) {
		if (code == name("eosio.token").value && (action == name("transfer").value)) {
			CGame2 thiscontract(name(receiver), name(code), datastream<const char*>(nullptr, 0));
			struct transfer_args {
				name from;
				name to;
				asset         quantity;
				string        memo;
			};
			auto unp_t = unpack_action_data<transfer_args>();
			thiscontract.apply_transfer(unp_t.from, unp_t.to, unp_t.quantity, unp_t.memo);
		}
	}
}
//authored by qpIlIpp (c) LetItPlay, 2019