#pragma once
//this file is intended to be included only by EvaluableNode.h

//This class implements a view of EvaluableNode that dispatches
//API requests for assoc data structures regardless of how it is stored in
//the corresponding EvaluableNode.  Because every byte counts with regard to
//EvaluableNode, this data structure leverages EvaluableNode's internal bit fields
//for its state and therefore contains a reference to the corresponding EvaluableNode.
//This view will adjust whether a SmallAssocType or LargeAssocType is employed.
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
	{ }

	AssocRef(const AssocRef &other) : en(other.en)
	{ }

	AssocRef &operator=(const AssocRef &other)
	{
		en = other.en;
		return *this;
	}

	//assign from a SmallAssocType
	inline AssocRef &operator=(SmallAssocType &other)
	{
		if(other.size() <= EvaluableNode::largestSmallAssocSize)
			ReduceToSmallIfPossible();
		else
			PromoteToLarge();

		if(IsSmall())
			GetSmallMap() = other;
		else
			GetLargeMap() = other;

		return *this;
	}

	//move from a SmallAssocType
	inline AssocRef &operator=(SmallAssocType &&other) noexcept
	{
		if(other.size() <= EvaluableNode::largestSmallAssocSize)
			ReduceToSmallIfPossible();
		else
			PromoteToLarge();

		if(IsSmall())
			GetSmallMap() = std::move(other);
		else
			GetLargeMap() = std::move(other);

		return *this;
	}

	//assign from a LargeAssocType
	inline AssocRef &operator=(LargeAssocType &other)
	{
		if(other.size() <= EvaluableNode::largestSmallAssocSize)
			ReduceToSmallIfPossible();
		else
			PromoteToLarge();

		if(IsSmall())
			GetSmallMap() = other.GetVectorMap();
		else
			GetLargeMap() = other;

		return *this;
	}

	//move from a LargeAssocType
	inline AssocRef &operator=(LargeAssocType &&other) noexcept
	{
		if(other.size() <= EvaluableNode::largestSmallAssocSize)
			ReduceToSmallIfPossible();
		else
			PromoteToLarge();

		if(IsSmall())
			GetSmallMap() = other.ExtractVectorMap();
		else
			GetLargeMap() = std::move(other);

		return *this;
	}

	//allow it to be used in context of EvaluableNode::SmallAssocType
	inline operator EvaluableNode::SmallAssocType &()
	{
		if(IsSmall())
			return GetSmallMap();
		else
			return GetLargeMap();
	}

	inline operator const EvaluableNode::SmallAssocType &() const
	{
		if(IsSmall())
			return GetSmallMap();
		else
			return GetLargeMap();
	}

	//--- Iterators ---

	inline iterator begin() noexcept
	{
		return IsSmall() ? GetSmallMap().begin() : GetLargeMap().begin();
	}

	inline const_iterator cbegin() const noexcept
	{
		return IsSmall() ? GetSmallMap().cbegin() : GetLargeMap().cbegin();
	}

	inline iterator end() noexcept
	{
		return IsSmall() ? GetSmallMap().end() : GetLargeMap().end();
	}

	inline const_iterator cend() const noexcept
	{
		return IsSmall() ? GetSmallMap().cend() : GetLargeMap().cend();
	}

	//--- Capacity & Size ---

	inline bool empty() const noexcept
	{
		return IsSmall() ? GetSmallMap().empty() : GetLargeMap().empty();
	}

	inline size_type size() const noexcept
	{
		return IsSmall() ? GetSmallMap().size() : GetLargeMap().size();
	}

	inline size_type max_size() const noexcept
	{
		return IsSmall() ? GetSmallMap().max_size() : GetLargeMap().max_size();
	}

	//--- Modifiers ---

	inline void reserve(size_type n)
	{
		if(IsSmall())
		{
			if(n <= EvaluableNode::largestSmallAssocSize)
			{
				GetSmallMap().reserve(n);
				return;
			}

			PromoteToLarge();
		}

		GetLargeMap().reserve(n);
	}

	inline void clear()
	{
		if(IsSmall())
			return GetSmallMap().clear();
		else
			return GetLargeMap().clear();
	}

	template<class... Args>
	inline std::pair<iterator, bool> try_emplace(const key_type &key, Args &&...args)
	{
		if(IsSmall())
		{
			auto result = GetSmallMap().try_emplace(key, std::forward<Args>(args)...);

			if(GetSmallMap().size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();

			return result;
		}
		else
		{
			return GetLargeMap().try_emplace(key, std::forward<Args>(args)...);
		}
	}

	template<class... Args>
	inline std::pair<iterator, bool> emplace(Args &&...args)
	{
		if(IsSmall())
		{
			auto result = GetSmallMap().emplace(std::forward<Args>(args)...);

			if(GetSmallMap().size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();

			return result;
		}
		else
		{
			return GetLargeMap().emplace(std::forward<Args>(args)...);
		}
	}

	inline std::pair<iterator, bool> insert_or_assign(const key_type &key, mapped_type &&value)
	{
		if(IsSmall())
		{
			auto result = GetSmallMap().insert_or_assign(key, std::move(value));

			if(GetSmallMap().size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();

			return result;
		}
		else
		{
			return GetLargeMap().insert_or_assign(key, std::move(value));
		}
	}

	inline std::pair<iterator, bool> insert(const value_type &value)
	{
		if(IsSmall())
		{
			auto result = GetSmallMap().insert(value);

			if(GetSmallMap().size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();

			return result;
		}
		else
		{
			return GetLargeMap().insert(value);
		}
	}

	inline std::pair<iterator, bool> insert(const key_type &key, const mapped_type &value)
	{
		if(IsSmall())
		{
			auto result = GetSmallMap().insert(key, value);

			if(GetSmallMap().size() > EvaluableNode::largestSmallAssocSize)
				PromoteToLarge();

			return result;
		}
		else
		{
			return GetLargeMap().insert(key, value);
		}
	}

	inline size_t erase(const key_type &key)
	{
		if(IsSmall())
		{
			return GetSmallMap().erase(key);
		}
		else
		{
			return GetLargeMap().erase(key);
		}
	}

	inline iterator erase(iterator pos)
	{
		if(IsSmall())
		{
			return GetSmallMap().erase(pos);
		}
		else
		{
			return GetLargeMap().erase(pos);
		}
	}

	inline void swap(AssocRef &other) noexcept
	{
		if(IsSmall())
		{
			if(other.IsSmall())
			{
				GetSmallMap().swap(other.GetSmallMap());
			}
			else
			{
				auto this_small_mcn = std::move(GetSmallMap());
				auto other_large_mcn = std::move(other.GetLargeMap());

				PromoteToLarge();
				GetLargeMap() = std::move(other_large_mcn);

				other.ReduceToSmallIfPossible();
				if(other.IsSmall())
					other.GetSmallMap() = std::move(this_small_mcn);
				else
					other.GetLargeMap() = std::move(this_small_mcn);
			}
		}
		else //!IsSmall()
		{
			if(other.IsSmall())
			{
				auto this_large_mcn = std::move(GetLargeMap());
				auto other_small_mcn = std::move(other.GetSmallMap());

				ReduceToSmallIfPossible();
				if(IsSmall())
					GetSmallMap() = std::move(other_small_mcn);
				else
					GetLargeMap() = std::move(other_small_mcn);

				other.PromoteToLarge();
				other.GetLargeMap()  = std::move(this_large_mcn);
			}
			else
			{
				GetLargeMap().swap(other.GetLargeMap());
			}
		}
	}

	//--- Lookup ---

	inline mapped_type &at(const key_type &key)
	{
		if(IsSmall())
			return GetSmallMap().at(key);
		else
			return GetLargeMap().at(key);
	}

	mapped_type &operator[](const key_type &key)
	{
		if(IsSmall())
		{
			auto &result = (GetSmallMap())[key];

			if(GetSmallMap().size() > EvaluableNode::largestSmallAssocSize)
			{
				PromoteToLarge();
				//need to re-retrieve
				return GetLargeMap()[key];
			}
			else
			{
				return result;
			}

		}
		else
		{
			return GetLargeMap()[key];
		}
	}

	inline size_type count(const key_type &key) const
	{
		if(IsSmall())
			return GetSmallMap().count(key);
		else
			return GetLargeMap().count(key);
	}

	inline iterator find(const key_type &key)
	{
		if(IsSmall())
			return GetSmallMap().find(key);
		else
			return GetLargeMap().find(key);
	}

	inline bool contains(const key_type &key) const
	{
		if(IsSmall())
			return GetSmallMap().contains(key);
		else
			return GetLargeMap().contains(key);
	}

	//used for more advanced manipulation
	inline std::vector<std::pair<key_type, mapped_type>> &GetVector()
	{
		if(IsSmall())
			return GetSmallMap().GetVector();
		else
			return GetLargeMap().GetVector();
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

	inline void ReduceToSmallIfPossible()
	{
		en->RemoveExtendedValueIfPossible();
	}

	inline EvaluableNode::SmallAssocType &GetSmallMap() const
	{
		return en->value.mappedChildNodes;
	}

	inline EvaluableNode::LargeAssocType &GetLargeMap() const
	{
		return *en->value.extendedMappedChildNodes.mappedChildNodes.get();
	}

	EvaluableNode *en;
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
