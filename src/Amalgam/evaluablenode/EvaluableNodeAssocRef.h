#pragma once
//this file is intended to be included only by EvaluableNode.h

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
		//TODO 25910: finish this
		if(other.IsSmall())
		{
			this->storage.smallMap = other.storage.smallMap;
		}
		else
		{
			this->storage.largeMap = std::make_unique<EvaluableNode::LargeAssocType>(*other.storage.largeMap);
		}
	}

	AssocRef &operator=(const AssocRef &other)
	{
		//TODO 25910: finish this
		if(this != &other)
		{
			en = other.en;
			if(other.IsSmall())
			{
				storage.smallMap = other.storage.smallMap;
			}
			else
			{
				storage.largeMap = std::make_unique<EvaluableNode::LargeAssocType>(*other.storage.largeMap);
			}
		}
		return *this;
	}

	//--- Iterators ---

	inline iterator begin() noexcept
	{
		return IsSmall() ? storage.smallMap->begin() : storage.largeMap->begin();
	}

	inline const_iterator cbegin() const noexcept
	{
		return IsSmall() ? storage.smallMap->cbegin() : storage.largeMap->cbegin();
	}

	inline iterator end() noexcept
	{
		return IsSmall() ? storage.smallMap->end() : storage.largeMap->end();
	}

	inline const_iterator cend() const noexcept
	{
		return IsSmall() ? storage.smallMap->cend() : storage.largeMap->cend();
	}

	//--- Capacity & Size ---

	inline bool empty() const noexcept
	{
		return IsSmall() ? storage.smallMap->empty() : storage.largeMap->empty();
	}

	inline size_type size() const noexcept
	{
		return IsSmall() ? storage.smallMap->size() : storage.largeMap->size();
	}

	inline size_type max_size() const noexcept
	{
		return IsSmall() ? storage.smallMap->max_size() : storage.largeMap->max_size();
	}

	//--- Modifiers ---

	//TODO 25910: finish this section (modifiers)
	void insert(const key_type &key, mapped_type value)
	{
		if(IsSmall())
		{
			if(storage.smallMap->size() >= 8)
			{
				PromoteToLarge();
			}
			storage.smallMap->insert(key, value);
		}
		else
		{
			storage.largeMap->insert(key, value);
		}
	}

	//--- Lookup ---

	inline mapped_type &at(const key_type &key)
	{
		if(IsSmall())
			return storage.smallMap->at(key);
		else
			return storage.largeMap->at(key);
	}

	mapped_type &operator[](const key_type &key)
	{
		if(IsSmall())
			return (*storage.smallMap)[key];
		else
			return (*storage.largeMap)[key];
	}

	inline size_type count(const key_type &key) const
	{
		if(IsSmall())
			return storage.smallMap->count(key);
		else
			return storage.largeMap->count(key);
	}

	inline iterator find(const key_type &key)
	{
		if(IsSmall())
			return storage.smallMap->find(key);
		else
			return storage.largeMap->find(key);
	}

	inline bool contains(const key_type &key) const
	{
		if(IsSmall())
			return storage.smallMap->contains(key);
		else
			return storage.largeMap->contains(key);
	}

private:	

	inline bool IsSmall() const
	{
		return !en->HasExtendedValue();
	}

	inline void PromoteToLarge()
	{
		en->EnsureHasExtendedValue();
	}

	EvaluableNode *en;

	union
	{
		EvaluableNode::SmallAssocType *smallMap;
		EvaluableNode::LargeAssocType *largeMap;
	} storage;
};
