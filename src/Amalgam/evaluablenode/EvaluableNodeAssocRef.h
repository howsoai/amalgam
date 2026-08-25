#pragma once
//this file is intended to be included only by EvaluableNode.h

//TODO 25910: finish this
/*
class EvaluableNode::AssocRef
{
public:
	using key_type = EvaluableNode::SmallAssocType::key_type;
	using mapped_type = EvaluableNode::SmallAssocType::mapped_type;
	using value_type = EvaluableNode::SmallAssocType::value_type;
	using size_type = std::size_t;

	using key_equal = EvaluableNode::SmallAssocType::key_equal;
	using iterator = typename EvaluableNode::SmallAssocType::iterator;
	using const_iterator = typename EvaluableNode::SmallAssocType::const_iterator;

	//--- Construction & Lifecycle ---

	AssocRef(EvaluableNode *_en) : en(_en)
	{}

	AssocRef(const AssocRef &other) : en(other.en)
	{
		if(other.IsSmall())
		{
			this->smallMap = other.smallMap;
		}
		else
		{
			this->largeMap = std::make_unique<EvaluableNode::LargeAssocType>(*other.largeMap);
		}
	}

	AssocRef &operator=(const AssocRef &other)
	{
		if(this != &other)
		{
			en = other.en;
			if(other.IsSmall())
			{
				smallMap = other.smallMap;
			}
			else
			{
				largeMap = std::make_unique<EvaluableNode::LargeAssocType>(*other.largeMap);
			}
		}
		return *this;
	}

	//--- Iterators ---

	//--- Capacity & Size ---


	//--- Modifiers ---

	//--- Lookup ---

	size_t size() const
	{
		return IsSmall() ? smallMap.size() : largeMap->size();
	}

	IterType begin() const
	{
		return IsSmall() ? smallMap.begin() : largeMap->begin();
	}

	IterType end() const
	{
		return IsSmall() ? smallMap.end() : largeMap->end();
	}

	void insert(const KeyType &key, ValueType value)
	{
		if(IsSmall())
		{
			if(smallMap.size() >= 8)
			{
				promote();
			}
			smallMap.insert(key, value);
		}
		else
		{
			largeMap->insert(key, value);
		}
	}

	ValueType &operator[](const KeyType &key)
	{
		if(IsSmall())
		{
			return smallMap[key];
		}
		else
		{
			return (*largeMap)[key];
		}
	}

private:	

	inline bool IsSmall() const
	{
		return !en->HasExtendedValue();
	}

	void promote()
	{
		auto new_map = std::make_unique<EvaluableNode::LargeAssocType<CompactlargeMap, CompactHashSet, KeyType, ValueType>>();
		for(auto &item : smallMap.data)
			newMap->insert(item.first, item.second);

		en->SetHasSmallAssoc(false);

		largeMap = std::move(new_map);
	}

	EvaluableNode *en;

	union
	{
		EvaluableNode::SmallAssocType *smallMap;
		EvaluableNode::LargeAssocType *largeMap;
	} storage;
};

//*/