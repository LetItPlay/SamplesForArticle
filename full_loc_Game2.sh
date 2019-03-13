eosio-cpp -abigen ContractGame2.cpp -o CGame2.wasm
mv CGame2.wasm ./bin/CGame2/CGame2.wasm
mv CGame2.abi  ./bin/CGame2/CGame2.abi

cleos set contract game2 ./bin/CGame2/ -p game2