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
	{
		if(!en->HasExtendedValue())
			storage.smallMap = &en->value.mappedChildNodes;
		else
			storage.largeMap = en->value.extendedMappedChildNodes.mappedChildNodes.get();
	}

	AssocRef(const AssocRef &other) : en(other.en)
	{
		if(other.IsSmall())
			this->storage.smallMap = other.storage.smallMap;
		else
			this->storage.largeMap = other.storage.largeMap;
	}

	AssocRef &operator=(const AssocRef &other)
	{
		if(this != &other)
		{
			en = other.en;
			if(other.IsSmall())
				storage.smallMap = other.storage.smallMap;
			else
				storage.largeMap = other.storage.largeMap;
		}
		return *this;
	}

	//allow it to be used in context of EvaluableNode::SmallAssocType
	inline operator EvaluableNode::SmallAssocType &()
	{
		if(IsSmall())
			return *storage.smallMap;
		else
			return *storage.largeMap;
	}

	inline operator const EvaluableNode::SmallAssocType &() const
	{
		if(IsSmall())
			return *storage.smallMap;
		else
			return *storage.largeMap;
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

	inline void reserve(size_type n)
	{
		if(IsSmall())
			return storage.smallMap->reserve(n);
		else
			return storage.largeMap->reserve(n);
	}

	inline void clear()
	{
		if(IsSmall())
			return storage.smallMap->clear();
		else
			return storage.largeMap->clear();
	}

	template<class... Args> inline std::pair<iterator, bool> try_emplace(const key_type &key, Args &&...args)
	{
		if(IsSmall())
		{
			return storage.smallMap->try_emplace(key, std::forward<Args>(args)...);

			if(storage.smallMap->size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();
		}
		else
		{
			return storage.largeMap->try_emplace(key, std::forward<Args>(args)...);
		}
	}

	template<class... Args> inline std::pair<iterator, bool> emplace(Args &&...args)
	{
		if(IsSmall())
		{
			return storage.smallMap->emplace(std::forward<Args>(args)...);

			if(storage.smallMap->size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();
		}
		else
		{
			return storage.largeMap->emplace(std::forward<Args>(args)...);
		}
	}

	inline std::pair<iterator, bool> insert_or_assign(const key_type &key, mapped_type &&value)
	{
		if(IsSmall())
		{
			return storage.smallMap->insert_or_assign(key, std::move(value));

			if(storage.smallMap->size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();
		}
		else
		{
			return storage.largeMap->insert_or_assign(key, std::move(value));
		}
	}

	inline std::pair<iterator, bool> insert(const value_type &value)
	{
		if(IsSmall())
		{
			return storage.smallMap->insert(value);

			if(storage.smallMap->size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();
		}
		else
		{
			return storage.largeMap->insert(value);
		}
	}

	inline std::pair<iterator, bool> insert(const key_type &key, const mapped_type &value)
	{
		if(IsSmall())
		{
			return storage.smallMap->insert(key, value);

			if(storage.smallMap->size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();
		}
		else
		{
			return storage.largeMap->insert(key, value);
		}
	}

	size_t erase(const key_type &key)
	{
		if(IsSmall())
		{
			return storage.smallMap->erase(key);
		}
		else
		{
			return storage.largeMap->erase(key);
		}
	}

	iterator erase(iterator pos)
	{
		if(IsSmall())
		{
			return storage.smallMap->erase(pos);
		}
		else
		{
			return storage.largeMap->erase(pos);
		}
	}

	inline void swap(AssocRef &other) noexcept
	{
		if(IsSmall())
		{
			if(other.IsSmall())
			{
				storage.smallMap->swap(*other.storage.smallMap);
			}
			else
			{
				auto this_small_mcn = std::move(storage.smallMap);
				auto other_large_mcn = std::move(*storage.largeMap);


				//TODO 25910: finish this
			}
		}
		else //!IsSmall()
		{
			if(other.IsSmall())
			{
				auto this_large_mcn = std::move(*storage.largeMap);
				auto other_small_mcn = std::move(storage.smallMap);

				//TODO 25910: finish this
			}
			else
			{
				storage.largeMap->swap(*other.storage.largeMap);
			}
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
		{
			auto &result = (*storage.smallMap)[key];

			if(storage.smallMap->size() > EvaluableNode::largestSmallAssocSize)
			{
				PromoteToLarge();
				//need to re-retrieve
				return (*storage.largeMap)[key];
			}
			else
			{
				return result;
			}

		}
		else
		{
			return (*storage.largeMap)[key];
		}
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
		storage.largeMap = en->value.extendedMappedChildNodes.mappedChildNodes.get();
	}

	EvaluableNode *en;

	union
	{
		EvaluableNode::SmallAssocType *smallMap;
		EvaluableNode::LargeAssocType *largeMap;
	} storage;
};

namespace
{
	inline auto begin(EvaluableNode::AssocRef &m)
	{
		return m.begin();
	}

	inline auto cbegin(const EvaluableNode::AssocRef &m)
	{
		return m.cbegin();
	}

	inline auto end(EvaluableNode::AssocRef &m)
	{
		return m.end();
	}

	inline auto cend(const EvaluableNode::AssocRef &m)
	{
		return m.cend();
	}
}
