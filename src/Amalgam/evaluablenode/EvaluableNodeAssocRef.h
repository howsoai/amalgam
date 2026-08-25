#pragma once
//this file is intended to be included only by EvaluableNode.h

//TODO 25910: finish this
/*
class EvaluableNode::AssocRef
{
public:
	using KeyType = Internal::KeyType;
	using ValueType = Internal::ValueType;

	using IterType = typename VectorMap<KeyType, ValueType>::iterator;

private:
	EvaluableNode *owner;

	bool IsSmall() const
	{
		return owner->HasSmallAssoc();
	}

	void promote()
	{
		auto new_map = std::make_unique<OrderedHashMap<CompactHashMap, CompactHashSet, KeyType, ValueType>>();
		for(auto &item : vectorMap.data)
			newMap->insert(item.first, item.second);

		owner->SetHasSmallAssoc(false);

		hashMap = std::move(new_map);
	}

public:

	AssocRef(EvaluableNode *owner_ptr) : owner(owner_ptr)
	{}

	AssocRef(const AssocRef &other) : owner(other.owner)
	{
		if(other.IsSmall())
		{
			this->vectorMap = other.vectorMap;
		}
		else
		{
			this->hashMap = std::make_unique<OrderedHashMap<...>>(*other.hashMap);
		}
	}

	AssocRef &operator=(const AssocRef &other)
	{
		if(this != &other)
		{
			owner = other.owner;
			if(other.IsSmall())
			{
				vectorMap = other.vectorMap;
			}
			else
			{
				hashMap = std::make_unique<OrderedHashMap<...>>(*other.hashMap);
			}
		}
		return *this;
	}

	size_t size() const
	{
		return IsSmall() ? vectorMap.size() : hashMap->size();
	}

	IterType begin() const
	{
		return IsSmall() ? vectorMap.begin() : hashMap->begin();
	}

	IterType end() const
	{
		return IsSmall() ? vectorMap.end() : hashMap->end();
	}

	void insert(const KeyType &key, ValueType value)
	{
		if(IsSmall())
		{
			if(vectorMap.size() >= 8)
			{
				promote();
			}
			vectorMap.insert(key, value);
		}
		else
		{
			hashMap->insert(key, value);
		}
	}

	ValueType &operator[](const KeyType &key)
	{
		if(IsSmall())
		{
			return vectorMap[key];
		}
		else
		{
			return (*hashMap)[key];
		}
	}

	union
	{
		VectorMapType *vectorMap;
		OrderedHashMapType *hashMap;
	} storage;
};

*/