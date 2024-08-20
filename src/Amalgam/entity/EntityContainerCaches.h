#pragma once

//project headers:
#include "EvaluableNode.h"

//forward declarations:
class Entity;

class EntityContainerCaches
{
public:
	virtual ~EntityContainerCaches();

	//adds the entity to the cache
	// container should contain entity
	// entity_index is the index that the entity should be stored as
	virtual void AddEntity(Entity *e, size_t entity_index, bool batch_add = false) = 0;

	//like AddEntity, but removes the entity from the cache and reassigns entity_index_to_reassign to use the old
	// entity_index; for example, if entity_index 3 is being removed and 5 is the highest index, if entity_index_to_reassign is 5,
	// then this function will move the entity data that was previously in index 5 to be referenced by index 3 for all caches
	virtual void RemoveEntity(Entity *e, size_t entity_index, size_t entity_index_to_reassign, bool batch_remove = false) = 0;

	//updates all of the label values for entity e with index entity_index
	virtual void UpdateAllEntityLabels(Entity *entity, size_t entity_index) = 0;

	//like UpdateAllEntityLabels, but only updates labels for the keys of labels_updated
	virtual void UpdateEntityLabels(Entity *entity, size_t entity_index, EvaluableNode::AssocType &labels_updated) = 0;
};

