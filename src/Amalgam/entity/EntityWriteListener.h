#pragma once

//project headers:
#include "Entity.h"

//system headers:
#include <fstream>

//forward declarations:
class Entity;

class EntityWriteListener
{
public:
	//stores all writes to entities as a seq of direct_assigns
	//listening_entity is the entity to store the relative ids to
	//if retain_writes is true, then the listener will store the writes, and GetWrites() will return the list of all writes accumulated
	//if filename is not empty, then it will attempt to open the file and log all writes to that file, and then flush the file stream
	EntityWriteListener(Entity *listening_entity, bool retain_writes = false, const std::string &filename = std::string());

	~EntityWriteListener();

	void LogSystemCall(EvaluableNode *params);

	// LogPrint does not flush to allow bulk processing
	void LogPrint(std::string &print_string);

	void LogWriteLabelValueToEntity(Entity *entity, const StringInternPool::StringID label_name, EvaluableNode *value, bool direct_set);

	//TODO 22194: update AssetManager to have a corresponding method, and be sure to include accum
	//like LogWriteLabelValueToEntity but where the keys are the labels and the values correspond in the assoc specified by label_value_pairs
	void LogWriteLabelValuesToEntity(Entity *entity, EvaluableNode *label_value_pairs, bool direct_set);

	//logs the new entity root, assuming it has already been set
	void LogWriteToEntityRoot(Entity *entity);

	void LogCreateEntity(Entity *new_entity);

	void LogDestroyEntity(Entity *destroyed_entity);

	void LogSetEntityRandomSeed(Entity *entity, const std::string &rand_seed, bool deep_set);

	void FlushLogFile();

	//returns all writes that the listener was aware of
	constexpr EvaluableNode *GetWrites()
	{
		return storedWrites;
	}

protected:
	//builds an assignment opcode for target_entity
	EvaluableNode *BuildNewWriteOperation(EvaluableNodeType assign_type, Entity *target_entity);

	void LogCreateEntityRecurse(Entity *new_entity);

	//performs the write of the entry
	void LogNewEntry(EvaluableNode *new_entry, bool flush = true);

	Entity *listeningEntity;

	EvaluableNodeManager listenerStorage;

	EvaluableNode *storedWrites;
	std::ofstream logFile;

#ifdef MULTITHREAD_SUPPORT
	//mutex for writing to make sure everything is written in the same order
	Concurrency::SingleMutex mutex;
#endif
};
