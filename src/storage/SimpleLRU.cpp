#include "SimpleLRU.h"
#include <iostream>


namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put( const std::string &key, const std::string &value ){ 

	auto it = _lru_index.find(key);

	if( it != _lru_index.end() ){
	
		update( &it->second.get(), value );
		push_front( &it->second.get() );
	}
	else{
		lru_node* node = new lru_node{key, value, {}, {}};
		push_front( node );
		_lru_index.insert( {key, std::ref(*node)} );
	}		

	return true; 
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent( const std::string &key, const std::string &value ){

	if( _lru_index.find(key) == _lru_index.end() ){			

		lru_node* node = new lru_node{key, value, {}, {}};
		push_front( node );
		_lru_index.insert( {key, std::ref(*node)} );
		return true;
	}
       
	return false; 
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set( const std::string &key, const std::string &value ){

	auto it = _lru_index.find(key);

	if( it != _lru_index.end() ){
		
		update( &it->second.get(), value );
		return true;
	}	

	return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete( const std::string &key ){

	auto it = _lru_index.find(key);	
	if( it == _lru_index.end() )
		return false;

	lru_node* node = &it->second.get();
	_lru_index.erase(it);

	if( !_lru_head )
		return false;
	
	if( node->next )
		node->next->prev = node->prev;	
	else
		_lru_tail = _lru_tail->prev;	

	if( node->prev ){

		if ( node->next )
			node->prev->next = std::move(node->next);
		else
			node->prev->next = nullptr;
	}
	_cur_size -= (node->key.size() + node->value.size());
	return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get( const std::string &key, std::string &value ){

	auto it = _lru_index.find(key);
	
	if( it != _lru_index.end() ){

		lru_node& node = it->second.get();
		value = node.value;
		push_front( &node );

		return true;
	}

	return false; 
}

bool SimpleLRU::update( lru_node* node, const std::string &value ){

	_cur_size = _cur_size - node->value.size() + value.size();
	node->value = value;
}

bool SimpleLRU::push_front( lru_node* node ){

	if( !node )
		return false;

	if( !node->prev && !node->next ){
		
		_cur_size += node->key.size() + node->value.size();
		while( _cur_size > _max_size && _lru_head)
			pop_back();

		if( _cur_size > _max_size )
			return false;
	}

	if( node == _lru_head.get() )
		return true;

	if( _lru_head ){

		std::unique_ptr<lru_node> un_node;

		if( node->next )
			node->next->prev = node->prev;

		if( node->prev ){

			un_node = std::move(node->prev->next);

			node->prev->next = std::move(node->next);
			node->prev = nullptr;
		}
		else
			un_node.reset(node);

		_lru_head->prev = node;
		un_node->next = std::move(_lru_head);
		_lru_head = std::move(un_node);
	}
	else{
		_lru_head.reset(node);
		_lru_tail = _lru_head.get();
	}

	return true;
}

bool SimpleLRU::pop_back(){

	_lru_index.erase(_lru_tail->key);
	_cur_size -= (_lru_tail->key.size() + _lru_tail->value.size());

	if( _lru_head ){

		if( _lru_tail->prev ){

			_lru_tail = _lru_tail->prev;		
			_lru_tail->next = nullptr;
		}
		else{
			_lru_head.reset();
			_lru_tail = nullptr;
		}
	}
	else
		return false;

	return true;
}

void SimpleLRU::print_list(){

	lru_node* node = _lru_head.get();

	std::cout << "LIST BEGIN\n" << std::endl;
	std::cout << "map.size: " << _lru_index.size() << std::endl;
	std::cout << "from HEAD!" << std::endl;
	
	while( node != nullptr ){
		
		std::cout << "adress: " << node << std::endl;
		std::cout << "key: " << node->key << std::endl;
		std::cout << "value: " << node->value << std::endl;
		std::cout << "next: " << node->next.get() << std::endl;
		std::cout << "prev: " << node->prev << std::endl;
		std::cout << std::endl;

		node = node->next.get();
	}

	std::cout << "from tail!" << std::endl;	
	node = _lru_tail;
	while( node != nullptr ){
		
		std::cout << "adress: " << node << std::endl;
		std::cout << "key: " << node->key << std::endl;
		std::cout << "value: " << node->value << std::endl;
		std::cout << "next: " << node->next.get() << std::endl;
		std::cout << "prev: " << node->prev << std::endl;
		std::cout << std::endl;

		node = node->prev;
	}

	std::cout << "LIST END" << std::endl;

}

} // namespace Backend
} // namespace Afina
