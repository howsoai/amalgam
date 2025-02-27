#pragma once

//system headers:
#include <cstddef>

//used to manage pairs of distance and a reference
// where operations take place more frequently via distance first, such that cache access is optimized
// the default type is size_t for indices
template<typename ReferenceType = size_t>
class DistanceReferencePair
{
public:
	constexpr DistanceReferencePair()
		: distance(0), reference(0)
	{	}

	constexpr DistanceReferencePair(double _distance, ReferenceType _reference)
		: distance(_distance), reference(_reference)
	{	}

	constexpr bool operator <(const DistanceReferencePair<ReferenceType> &drp) const
	{
		return distance < drp.distance;
	}

	constexpr bool operator <=(const DistanceReferencePair<ReferenceType> &drp) const
	{
		return distance <= drp.distance;
	}

	constexpr bool operator ==(const DistanceReferencePair<ReferenceType> &drp) const
	{
		return distance == drp.distance;
	}

	constexpr bool operator ==(const double &value) const
	{
		return distance == value;
	}

	constexpr bool SameReference(const DistanceReferencePair<ReferenceType> &drp) const
	{
		return reference == drp.reference;
	}

	//returns a reference that will always be invalid, that should, for all practical purposes, always return
	// false if compared via equality against a valid reference
	static constexpr ReferenceType InvalidReference()
	{
		return static_cast<ReferenceType>(-1LL);
	}

	double distance;
	ReferenceType reference;
};

//related to distance index pair but for size_t as a computed feature count in addition to distance
template<typename ReferenceType = size_t>
class CountDistanceReferencePair
{
public:
	constexpr CountDistanceReferencePair(size_t _count, double _distance, ReferenceType _reference)
		: count(_count), distance(_distance), reference(_reference)
	{	}

	//a larger count means more has been computed, and minimum distance should be found
	// with the largest number of computed features
	constexpr bool operator <(const CountDistanceReferencePair &cdrp) const
	{
		if(count == cdrp.count)
			return distance < cdrp.distance;
		return count > cdrp.count;
	}

	//a larger count means more has been computed, and minimum distance should be found
	// with the largest number of computed features
	constexpr bool operator <=(const CountDistanceReferencePair &cdrp) const
	{
		if(count == cdrp.count)
			return distance <= cdrp.distance;
		return count >= cdrp.count;
	}

	constexpr bool operator ==(const CountDistanceReferencePair &cdrp) const
	{
		return count == cdrp.count && distance == cdrp.distance;
	}

	constexpr bool SameReference(const DistanceReferencePair<ReferenceType> &drp) const
	{
		return reference == drp.reference;
	}

	//returns a reference that will always be invalid, that should, for all practical purposes, always return
	// false if compared via equality against a valid reference
	static constexpr ReferenceType InvalidReference()
	{
		return static_cast<ReferenceType>(-1LL);
	}

	size_t count;
	double distance;
	ReferenceType reference;
};
