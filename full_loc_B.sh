eosio-cpp -abigen ContractB.cpp -o CHackB.wasm
mv CHackB.wasm ./bin/CHackB/CHackB.wasm
mv CHackB.abi  ./bin/CHackB/CHackB.abi

cleos set contract b ./bin/CHackB/ -p b