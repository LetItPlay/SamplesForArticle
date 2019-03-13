eosio-cpp -abigen ContractD.cpp -o CHackD.wasm
mv CHackD.wasm ./bin/CHackD/CHackD.wasm
mv CHackD.abi  ./bin/CHackD/CHackD.abi

cleos set contract d ./bin/CHackD/ -p d