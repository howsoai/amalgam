#pragma once

//project headers:
#include "Entity.h"

//system headers:
#include <fstream>

class EntityWriteListener
{
public:
	//stores all writes to entities as a seq of direct_assigns
	//listening_entity is the entity to store the relative ids to
	//if retain_writes is true, then the listener will store the writes, and GetWrites() will return the list of all writes accumulated
	//if filename is not empty, then it will attempt to open the file and log all writes to that file, and then flush the filestream
	EntityWriteListener(Entity *listening_entity, bool retain_writes = false, const std::string &filename = std::string());

	~EntityWriteListener();

	void LogSystemCall(EvaluableNode *params);

	// LogPrint does not flush to allow bulk processing
	void LogPrint(std::string &print_string);

	void LogWriteValueToEntity(Entity *entity, EvaluableNode *value, const StringInternPool::StringID label_name, bool direct_set);

	//like LogWriteValueToEntity but where the keys are the labels and the values correspond in the assoc specified by label_value_pairs
	void LogWriteValuesToEntity(Entity *entity, EvaluableNode *label_value_pairs, bool direct_set);

	void LogWriteToEntity(Entity *entity, const std::string &new_code);

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
