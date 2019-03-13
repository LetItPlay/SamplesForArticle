eosio-cpp -abigen ContractC.cpp -o CHackC.wasm
mv CHackC.wasm ./bin/CHackC/CHackC.wasm
mv CHackC.abi  ./bin/CHackC/CHackC.abi

cleos set contract c ./bin/CHackC/ -p c