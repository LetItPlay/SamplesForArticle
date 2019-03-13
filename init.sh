# 5KRCeddBePk4fr4Dug6X86fZ4Avtt3VA7kFArCX2eEGNXYXi2JH
# EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD

cleos wallet import --private-key 5KRCeddBePk4fr4Dug6X86fZ4Avtt3VA7kFArCX2eEGNXYXi2JH
cleos create account eosio a EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio b EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio c EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio d EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio e EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio game1 EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio game2 EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio game3 EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio game4 EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD
cleos create account eosio game5 EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD

cleos push action eosio.token issue '["a", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["b", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["c", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["d", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["e", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["game1", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["game2", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["game3", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["game4", "100.0000 EOS", "hi!"]' -p eosio
cleos push action eosio.token issue '["game5", "100.0000 EOS", "hi!"]' -p eosio

cleos set account permission d active '{"threshold": 1,"keys": [{"key": "EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD","weight": 1}],"accounts": [{"permission":{"actor":"d","permission":"eosio.code"},"weight":1}]}' owner -p d
cleos set account permission game1 active '{"threshold": 1,"keys": [{"key": "EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD","weight": 1}],"accounts": [{"permission":{"actor":"game1","permission":"eosio.code"},"weight":1}]}' owner -p game1
cleos set account permission game2 active '{"threshold": 1,"keys": [{"key": "EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD","weight": 1}],"accounts": [{"permission":{"actor":"game2","permission":"eosio.code"},"weight":1}]}' owner -p game2
cleos set account permission game3 active '{"threshold": 1,"keys": [{"key": "EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD","weight": 1}],"accounts": [{"permission":{"actor":"game3","permission":"eosio.code"},"weight":1}]}' owner -p game3
cleos set account permission game4 active '{"threshold": 1,"keys": [{"key": "EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD","weight": 1}],"accounts": [{"permission":{"actor":"game4","permission":"eosio.code"},"weight":1}]}' owner -p game4
cleos set account permission game5 active '{"threshold": 1,"keys": [{"key": "EOS6CjXrqJZDNHf5WVXKaqq3r6mCPv3QvwUa9VNU5X5Mjp8p3JBqD","weight": 1}],"accounts": [{"permission":{"actor":"game5","permission":"eosio.code"},"weight":1}]}' owner -p game5

mkdir bin
cd bin
mkdir CHackB
mkdir CHackC
mkdir CHackD
mkdir CGame1
mkdir CGame2

cd ..
./full_loc_B.sh
./full_loc_C.sh
./full_loc_D.sh
./full_loc_Game1.sh
./full_loc_Game2.sh