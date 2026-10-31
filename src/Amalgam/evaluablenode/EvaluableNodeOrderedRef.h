#pragma once
#include <vector>
#include <iterator>
#include <algorithm>

//This class implements a view of EvaluableNode that dispatches
//API requests for ordered data structures regardless of how it is stored in
//the corresponding EvaluableNode.
class EvaluableNode::OrderedRef
{
public:
	using value_type = EvaluableNode *;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = value_type &;
	using const_reference = const value_type &;
	using pointer = value_type *;
	using const_pointer = const value_type *;

	//use raw pointers to unify the iterator type across array and vector storage
	using iterator = EvaluableNode **;
	using const_iterator = const EvaluableNode **;

	//--- Construction & Lifecycle ---

	OrderedRef(EvaluableNode *_en) noexcept
		: en(_en)
	{}

	OrderedRef(const OrderedRef &other) noexcept
		: en(other.en)
	{}

	OrderedRef &operator=(const OrderedRef &other)
	{
		en = other.en;
		return *this;
	}

	//--- Iterators ---

	inline iterator begin() noexcept
	{
		return IsSmall() ? GetSmallNodes().data() : GetLargeNodes().data();
	}

	inline const_iterator cbegin() const noexcept
	{
		return IsSmall() ? GetSmallNodes().data() : GetLargeNodes().data();
	}

	inline iterator end() noexcept
	{
		return begin() + size();
	}

	inline const_iterator cend() const noexcept
	{
		return cbegin() + size();
	}

	//--- Capacity & Size ---

	inline bool empty() const noexcept
	{
		return size() == 0;
	}

	inline size_type size() const noexcept
	{
		return IsSmall() ? en->numElementsWhenSmall : GetLargeNodes().size();
	}

	inline size_type max_size() const noexcept
	{
		return IsSmall() ? EvaluableNode::smallOrderedTypeMaxSize : GetLargeNodes().max_size();
	}

	inline size_type capacity() const noexcept
	{
		return IsSmall() ? EvaluableNode::smallOrderedTypeMaxSize : GetLargeNodes().capacity();
	}

	inline void shrink_to_fit() noexcept
	{
		if(!IsSmall())
			GetLargeNodes().shrink_to_fit();
	}

	//--- Element Access ---

	inline reference operator[](size_type pos)
	{
		return IsSmall() ? GetSmallNodes()[pos] : GetLargeNodes()[pos];
	}

	inline const_reference operator[](size_type pos) const
	{
		return IsSmall() ? GetSmallNodes()[pos] : GetLargeNodes()[pos];
	}

	inline reference at(size_type pos)
	{
		return IsSmall() ? GetSmallNodes().at(pos) : GetLargeNodes().at(pos);
	}

	inline const_reference at(size_type pos) const
	{
		return IsSmall() ? GetSmallNodes().at(pos) : GetLargeNodes().at(pos);
	}

	inline reference front()
	{
		return IsSmall() ? GetSmallNodes()[0] : GetLargeNodes().front();
	}

	inline reference back()
	{
		return IsSmall() ? GetSmallNodes()[size() - 1] : GetLargeNodes().back();
	}

	inline pointer data()
	{
		return IsSmall() ? GetSmallNodes().data() : GetLargeNodes().data();
	}

	//--- Modifiers ---

	inline void clear()
	{
		if(IsSmall())
		{
			en->numElementsWhenSmall = 0;
		}
		else
		{
			GetLargeNodes().clear();
		}
	}

	inline void reserve(size_type n)
	{
		if(IsSmall())
		{
			if(n <= EvaluableNode::smallOrderedTypeMaxSize)
				return;

			PromoteToLarge();
		}
		GetLargeNodes().reserve(n);
	}

	inline void push_back(const value_type &val)
	{
		if(IsSmall())
		{
			if(en->numElementsWhenSmall < EvaluableNode::smallOrderedTypeMaxSize)
			{
				GetSmallNodes()[en->numElementsWhenSmall++] = val;
				return;
			}
			PromoteToLarge();
		}
		GetLargeNodes().push_back(val);
	}

	inline void pop_back()
	{
		if(IsSmall())
		{
			if(en->numElementsWhenSmall > 0)
				en->numElementsWhenSmall--;
		}
		else
		{
			GetLargeNodes().pop_back();
		}
	}

	template<class... Args>
	inline void emplace_back(Args &&...args)
	{
		if(IsSmall())
		{
			if(en->numElementsWhenSmall < EvaluableNode::smallOrderedTypeMaxSize)
			{
				GetSmallNodes()[en->numElementsWhenSmall++] = std::forward<T>(args)...;
				return;
			}
			PromoteToLarge();
		}
		GetLargeNodes().emplace_back(std::forward<T>(args)...);
	}

	inline void insert(const_iterator pos, const value_type &val)
	{
		if(IsSmall())
		{
			size_type index = std::distance(cbegin(), pos);
			if(en->numElementsWhenSmall < EvaluableNode::smallOrderedTypeMaxSize)
			{
				for(size_type i = en->numElementsWhenSmall; i > index; --i)
					GetSmallNodes()[i] = GetSmallNodes()[i - 1];
				GetSmallNodes()[index] = val;
				en->numElementsWhenSmall++;
				return;
			}
			PromoteToLarge();
		}

		// Convert iterator to offset and insert into the current large storage
		size_type index = std::distance(cbegin(), pos);
		GetLargeNodes().insert(GetLargeNodes().begin() + index, val);
	}

	inline iterator erase(const_iterator first, const_iterator last)
	{
		if(IsSmall())
		{
			size_type start = std::distance(cbegin(), first);
			size_type end = std::distance(cbegin(), last);
			size_type count = end - start;

			for(size_type i = start; i + count <= en->numElementsWhenSmall; ++i)
				GetSmallNodes()[i] = GetSmallNodes()[i + count];

			en->numElementsWhenSmall -= count;
			return begin() + start;
		}

		size_type start = std::distance(cbegin(), first);
		size_type end = std::distance(cbegin(), last);
		GetLargeNodes().erase(GetLargeNodes().begin() + start, GetLargeNodes().begin() + end);
		return begin() + start;
	}

	inline iterator erase(const_iterator pos)
	{
		return erase(pos, pos + 1);
	}

	inline void swap(OrderedRef &other)
	{
		if(IsSmall() && other.IsSmall())
		{
			std::swap(GetSmallNodes(), other.GetSmallNodes());
			std::swap(en->numElementsWhenSmall, other.en->numElementsWhenSmall);
		}
		else if(!IsSmall() && !other.IsSmall())
		{
			GetLargeNodes().swap(other.GetLargeNodes());
		}
		else
		{
			PromoteToLarge();
			other.PromoteToLarge();
			GetLargeNodes().swap(other.GetLargeNodes());
			ReduceToSmallIfPossible();
			other.ReduceToSmallIfPossible();
		}
	}

private:
	inline bool IsSmall() const
	{
		return !en->HasExtendedValue() && (en->numElementsWhenSmall <= EvaluableNode::smallOrderedTypeMaxSize);
	}

	inline void PromoteToLarge()
	{
		en->EnsureHasExtendedValue();
		size_type current_size = en->numElementsWhenSmall;
		auto &large_nodes = GetLargeNodes();
		auto &small_nodes = GetSmallNodes();
		large_nodes.reserve(current_size);
		for(size_type i = 0; i < current_size; i++)
			large_nodes.push_back(small_nodes[i]);
	}

	inline void ReduceToSmallIfPossible()
	{
		if(GetLargeNodes().size() <= EvaluableNode::smallOrderedTypeMaxSize)
		{
			auto &large_nodes = GetLargeNodes();
			auto &small_nodes = GetSmallNodes();
			for(size_type i = 0; i < large_nodes.size(); ++i)
				small_nodes[i] = large_nodes[i];

			en->numElementsWhenSmall = static_cast<uint8_t>(large_nodes.size());
			en->RemoveExtendedValueIfPossible();
		}
	}

	inline EvaluableNode::SmallOrderedtype &GetSmallNodes() const
	{
		return en->value.smallOrderedChildNodes;
	}

	inline EvaluableNode::LargeOrderedType &GetLargeNodes() const
	{
		if(!en->HasExtendedValue())
			return en->value.orderedChildNodes;
		else
			return *en->value.extendedOrderedChildNodes.orderedChildNodes.get();
	}

	EvaluableNode *en;
};

inline auto begin(EvaluableNode::OrderedRef &m)
{
	return m.begin();
}

inline auto cbegin(const EvaluableNode::OrderedRef &m)
{
	return m.cbegin();
}

inline auto end(EvaluableNode::OrderedRef &m)
{
	return m.end();
}

inline auto cend(const EvaluableNode::OrderedRef &m)
{
	return m.cend();
}
