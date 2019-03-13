eosio-cpp -abigen ContractGame1.cpp -o CGame1.wasm
mv CGame1.wasm ./bin/CGame1/CGame1.wasm
mv CGame1.abi  ./bin/CGame1/CGame1.abi

cleos set contract game1 ./bin/CGame1/ -p game1