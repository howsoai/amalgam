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
	using const_iterator = EvaluableNode * const *;

	//--- Construction & Lifecycle ---

	inline OrderedRef() noexcept
		: en(&EvaluableNode::emptyOrderedChildNodes)
	{}

	inline OrderedRef(EvaluableNode *_en) noexcept
		: en(_en)
	{}

	inline OrderedRef(const OrderedRef &other) noexcept
		: en(other.en)
	{}

	inline void SetEvaluableNodePointer(EvaluableNode *_en)
	{
		en = _en;
	}

	template<class InputIt>
	inline void assign(InputIt first, InputIt last)
	{
		auto n = std::distance(first, last);
		clear();
		reserve(static_cast<size_type>(n));
		for(; first != last; ++first)
			push_back(*first);
	}

	inline OrderedRef &operator=(OrderedRef &other)
	{
		if(this == &other)
			return *this;

		assign(other.begin(), other.end());

		return *this;
	}

	inline OrderedRef &operator=(OrderedRef &&other) noexcept
	{
		if(this == &other)
			return *this;

		if(!IsSmall() && !other.IsSmall())
			GetLargeNodes().swap(other.GetLargeNodes());
		else
			assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));

		return *this;
	}

	inline OrderedRef &operator=(const std::vector<value_type> &other)
	{
		assign(other.begin(), other.end());
		return *this;
	}

	inline OrderedRef &operator=(std::vector<value_type> &&other)
	{
		if(other.size() > EvaluableNode::smallOrderedTypeMaxSize)
		{
			PromoteToLarge();
			GetLargeNodes() = std::move(other);
		}
		else
		{
			assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
		}

		return *this;
	}

	//extracts the data into a LargeOrderedType, reusing memory if possible
	inline LargeOrderedType ExtractLargeOrderedType()
	{
		if(IsSmall())
			return LargeOrderedType(begin(), end());
		else
			return std::move(GetLargeNodes());
	}

	inline const pointer data() const
	{
		return IsSmall() ? GetSmallNodes().data() : GetLargeNodes().data();
	}

	inline bool HasValidNode() const noexcept
	{
		return (en != nullptr);
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

	inline void resize(size_type count)
	{
		if(IsSmall())
		{
			if(count > EvaluableNode::smallOrderedTypeMaxSize)
			{
				PromoteToLarge();
				GetLargeNodes().resize(count);
			}
			else
			{
				size_type current_size = en->numElementsWhenSmall;
				if(count > current_size)
				{
					for(size_type i = current_size; i < count; ++i)
						GetSmallNodes()[i] = nullptr;
				}
				en->numElementsWhenSmall = static_cast<uint8_t>(count);
			}
		}
		else
		{
			GetLargeNodes().resize(count);
		}
	}

	inline void resize(size_type count, const value_type &val)
	{
		if(IsSmall())
		{
			if(count > EvaluableNode::smallOrderedTypeMaxSize)
			{
				PromoteToLarge();
				GetLargeNodes().resize(count, val);
			}
			else
			{
				size_type current_size = en->numElementsWhenSmall;
				if(count > current_size)
				{
					for(size_type i = current_size; i < count; ++i)
						GetSmallNodes()[i] = val;
				}
				en->numElementsWhenSmall = static_cast<uint8_t>(count);
			}
		}
		else
		{
			GetLargeNodes().resize(count, val);
		}
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
		if(!IsSmall())
		{
			GetLargeNodes().clear();
			ReduceToSmallIfPossible();
		}
		else
		{
			en->numElementsWhenSmall = 0;
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
				std::construct_at(&GetSmallNodes()[en->numElementsWhenSmall++], std::forward<Args>(args)...);
				return;
			}
			PromoteToLarge();
		}
		GetLargeNodes().emplace_back(std::forward<Args>(args)...);
	}

	inline void insert(const_iterator pos, const value_type &val)
	{
		size_type index = std::distance(cbegin(), pos);
		if(IsSmall())
		{
			if(en->numElementsWhenSmall < EvaluableNode::smallOrderedTypeMaxSize)
			{
				for(size_type i = en->numElementsWhenSmall; i > index; i--)
					GetSmallNodes()[i] = GetSmallNodes()[i - 1];
				GetSmallNodes()[index] = val;
				en->numElementsWhenSmall++;
				return;
			}
			PromoteToLarge();
		}

		GetLargeNodes().insert(GetLargeNodes().begin() + index, val);
	}

	inline void insert(const_iterator pos, size_type count, const value_type &val)
	{
		size_type index = std::distance(cbegin(), pos);
		if(count == 0)
			return;

		if(IsSmall())
		{
			size_type current_size = en->numElementsWhenSmall;
			if(count <= EvaluableNode::smallOrderedTypeMaxSize - current_size)
			{
				auto &small_nodes = GetSmallNodes();

				for(size_type i = current_size; i > index; i--)
					small_nodes[i + count - 1] = small_nodes[i - 1];

				for(size_type i = 0; i < count; ++i)
					small_nodes[index + i] = val;

				en->numElementsWhenSmall += static_cast<uint8_t>(count);
				return;
			}

			PromoteToLarge();
		}

		GetLargeNodes().insert(GetLargeNodes().begin() + index, count, val);
	}


	template<class InputIt>
	inline void insert(const_iterator pos, InputIt first, InputIt last)
	{
		size_type index = std::distance(cbegin(), pos);
		auto n = std::distance(first, last);

		if(IsSmall())
		{
			if(static_cast<size_t>(en->numElementsWhenSmall + n) <= EvaluableNode::smallOrderedTypeMaxSize)
			{
				//shift any elements necessary
				size_type current_size = en->numElementsWhenSmall;
				auto &small_nodes = GetSmallNodes();
				for(size_type i = current_size; i > index; i--)
					small_nodes[i + n - 1] = small_nodes[i - 1];

				std::copy(first, last, small_nodes.begin() + index);
				en->numElementsWhenSmall += static_cast<uint8_t>(n);
				return;
			}
			PromoteToLarge();
		}

		GetLargeNodes().insert(GetLargeNodes().begin() + index, first, last);
	}

	inline iterator erase(const_iterator first, const_iterator last)
	{
		if(IsSmall())
		{
			size_type start = std::distance(cbegin(), first);
			size_type end = std::distance(cbegin(), last);
			size_type count = end - start;
			auto &small_nodes = GetSmallNodes();

			for(size_type i = start; i + count < en->numElementsWhenSmall; i++)
				small_nodes[i] = small_nodes[i + count];

			en->numElementsWhenSmall -= static_cast<uint8_t>(count);
			return begin() + start;
		}

		size_type start = std::distance(cbegin(), first);
		size_type end = std::distance(cbegin(), last);
		auto &large_nodes = GetLargeNodes();
		large_nodes.erase(large_nodes.begin() + start, large_nodes.begin() + end);
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
		return (!en->HasExtendedValue() && !en->HasLargeOrderedType());
	}

	inline void PromoteToLarge()
	{
		en->EnsureHasLargeOrderedType();
	}

	inline void ReduceToSmallIfPossible()
	{
		en->ReduceToSmallOrderedTypeIfPossible();
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

namespace std::ranges
{
	template<> inline constexpr bool enable_borrowed_range<EvaluableNode::OrderedRef> = true;
}
