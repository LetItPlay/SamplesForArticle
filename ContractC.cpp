#include <eosiolib/eosio.hpp>
#include <eosiolib/system.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/asset.hpp>

using namespace eosio;
using namespace std;

const name from_acc = name("a");
const name game_acc = name("game2");

extern "C" {
	void apply(uint64_t receiver, uint64_t code, uint64_t action) {
		if (code == name("eosio.token").value && (action == name("transfer").value)) {
			struct transfer_args {
				name from;
				name to;
				asset         quantity;
				string        memo;
			};
			auto unp_t = unpack_action_data<transfer_args>();
			if (unp_t.from == from_acc)
			{
				require_recipient(game_acc);
			}
		}
	}
}
//authored by qpIlIpp (c) LetItPlay, 2019