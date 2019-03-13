#include <eosiolib/eosio.hpp>
#include <eosiolib/system.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.h>

using namespace eosio;
using namespace std;

const name my_acc = name("d");
const name game_acc = name("game2");


uint8_t get_roll() {
	/*
  j0 = (*Z_envZ_current_timeZ_jv)();
  l14 = j0;
  i0 = (*Z_envZ_tapos_block_prefixZ_iv)();
  l6 = i0;
  i0 = 8619u;
  (*Z_envZ_printsZ_vi)(i0);
  i0 = l6;
  j1 = l14;
  j2 = 1000000ull;
  j1 = DIV_U(j1, j2);
  i1 = (u32)(j1);
  i0 += i1;
  i1 = 179424691u;
  i0 *= i1;
  i1 = 100u;
  i0 = REM_U(i0, i1);
	*/
	uint64_t l3 = 0;
	uint32_t i0, i1;
	uint64_t j0, j1, j2, l14, l6;

	j0 = current_time(); // Here we've got current_time
	l14 = j0;
	i0 = tapos_block_prefix(); // Here we've got tapos_block_prefix
	l6 = i0;
	i0 = 8619u;
	//(*Z_envZ_printsZ_vi)(i0); // some debug print
	i0 = l6;
	j1 = l14;
	j2 = 1000000ull;
	j1 = j1 / j2; //here we got now (c_t /= 1000000)
	i1 = (uint32_t)(j1);
	i0 += i1; //heere's their sum
	i1 = 179424691u;
	i0 *= i1; //here's our random seed
	return i0 % 100 + 1;
}

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
			if ((unp_t.from == my_acc) && (unp_t.to == game_acc))
			{
				auto roll = get_roll();
				print("expected roll is ", roll);
				eosio_assert(roll <= 50, "you will not win that game!");
			}
		}
	}
}
//authored by qpIlIpp (c) LetItPlay, 2019