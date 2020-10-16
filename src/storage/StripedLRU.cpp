#include "StripedLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {


// See MapBasedGlobalLockImpl.h
bool StripedLRU::Put( const std::string &key, const std::string &value ){ 

	size_t shard_num = hash_func(key) % _stripe_count;		
	return shard[shard_num]->Put( key, value );		
}


// See MapBasedGlobalLockImpl.h
bool StripedLRU::PutIfAbsent( const std::string &key, const std::string &value ){

	size_t shard_num = hash_func(key) % _stripe_count;
	return shard[shard_num]->PutIfAbsent( key, value );       
}


// See MapBasedGlobalLockImpl.h
bool StripedLRU::Set( const std::string &key, const std::string &value ){

	size_t shard_num = hash_func(key) % _stripe_count;
	return shard[shard_num]->Set( key, value );       
}


// See MapBasedGlobalLockImpl.h
bool StripedLRU::Delete( const std::string &key ){

	size_t shard_num = hash_func(key) % _stripe_count;
	return shard[shard_num]->Delete( key );	
}


// See MapBasedGlobalLockImpl.h
bool StripedLRU::Get( const std::string &key, std::string &value ){

	size_t shard_num = hash_func(key) % _stripe_count;
	return shard[shard_num]->Get( key, value );
}


} // namespace Backend
} // namespace Afina
