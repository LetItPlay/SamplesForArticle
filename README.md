Samples for medium article

eos is avaliable here : https://github.com/EOSIO/eos

eosio.cdt (building contract) is avaliable here : https://github.com/EOSIO/eosio.cdt

wasm2c is avaliable here : https://github.com/WebAssembly/wabt

initialization:
init.sh

sources for contracts:

ContractB.cpp

ContractC.cpp

ContractD.cpp

ContractGame2.cpp

ContractGame2.cpp

time line of transactions after init here: 
commands.sh

decompilation info in folder 

./decompile

decompiled.c - file with full contract game2 decompilation

decompiled_short.c contains only decompilation of 
```
extern "C" {
	uint64_t get_rand() {
		return (now() + tapos_block_prefix()) * 179424691u % 0x0fedcba7afffffff; //some magic values
	}
	void apply(uint64_t receiver, uint64_t code, uint64_t action) {
		auto x = get_rand();
		print(x);
	}
}
```

Random in decompiled.c code:
```
j0 = (*Z_envZ_current_timeZ_jv)(); // Here we've got current_time
l14 = j0;
i0 = (*Z_envZ_tapos_block_prefixZ_iv)(); // Here we've got tapos_block_prefix
l6 = i0;
i0 = 8619u;
(*Z_envZ_printsZ_vi)(i0); // some debug print
i0 = l6;
j1 = l14;
j2 = 1000000ull;
j1 = DIV_U(j1, j2); //here we got now (c_t /= 1000000)
i1 = (u32)(j1);
i0 += i1; //heere's their sum
i1 = 179424691u;
i0 *= i1; //here's our random seed
i1 = 100u;
i0 = REM_U(i0, i1); // here's our roll
```
The same in decompiled_short.c:
```
u64 l3 = 0;
FUNC_PROLOGUE;
u32 i0, i1;
u64 j0, j1, j2;
f8();
j0 = (*Z_envZ_current_timeZ_jv)();
l3 = j0;
i0 = (*Z_envZ_tapos_block_prefixZ_iv)();
j1 = l3;
j2 = 1000000ull;
j1 = DIV_U(j1, j2);
i1 = (u32)(j1);
i0 += i1;
i1 = 179424691u;
i0 *= i1;
j0 = (u64)(i0);
(*Z_envZ_printuiZ_vj)(j0);
```
#authored by qpIlIpp (c) LetItPlay, 2019