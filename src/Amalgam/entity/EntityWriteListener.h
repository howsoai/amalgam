#pragma once

//project headers:
#include "BinaryPacking.h"
#include "Entity.h"

//system headers:
#include <fstream>

//forward declarations:
class Entity;

class EntityWriteListener
{
public:
	//stores all writes to entities as a seq of assignments
	//listening_entity is the entity to store the relative ids to
	//if retain_writes is true, then the listener will store the writes, and GetWrites() will return the list of all writes accumulated
	//if _pretty is true, then the listener will pretty print to filename
	//if sort_keys is true, then the listener will print with keys sorted for assocs
	//if transaction_file is not empty, then it takes ownership of the file, logging all writes and flushing the stream after each
	EntityWriteListener(Entity *listening_entity, std::unique_ptr<std::ostream> &&transaction_file, bool retain_writes = false,
		bool _pretty = false, bool sort_keys = false);

	//stores all writes, appending them to transaction_file
	//if huffman_tree is not null, the write listener will assume ownership of the memory and use it to compress output
	EntityWriteListener(Entity *listening_entity, bool _pretty, bool sort_keys,
		std::unique_ptr<std::ostream> &&transaction_file, HuffmanTree<uint8_t> *huffman_tree = nullptr);

	~EntityWriteListener();

	void LogSystemCall(EvaluableNode *params);

	// LogPrint does not flush to allow bulk processing
	void LogPrint(std::string &print_string);

	void LogWriteLabelValueToEntity(Entity *entity, const StringInternPool::StringID label_name, EvaluableNode *value);

	//like LogWriteLabelValueToEntity but where the keys are the labels and the values correspond
	// in the assoc specified by label_value_pairs
	void LogWriteLabelValuesToEntity(Entity *entity, EvaluableNode *label_value_pairs, bool accum_values);

	void LogRemoveLabesFromEntity(Entity *entity, EvaluableNode *labels);

	//logs the new entity root, assuming it has already been set
	void LogWriteToEntityRoot(Entity *entity);

	//logs accum to the entity root
	void LogEntityAccumRoot(Entity *entity, EvaluableNodeReference accum_code);

	void LogCreateEntity(Entity *new_entity);

	void LogDestroyEntity(Entity *destroyed_entity);

	void LogSetEntityRandomSeed(Entity *entity, const std::string &rand_seed, bool deep_set);

	void LogSetEntityPermissions(Entity *entity,
		EntityPermissions permissions_to_set, EntityPermissions permission_values, bool deep_set);

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
	std::unique_ptr<std::ostream> logFile;
	//used for compressing output if not nullptr; this memory is managed by this listener and must be freed
	HuffmanTree<uint8_t> *huffmanTree;

#ifdef MULTITHREAD_SUPPORT
	//mutex for writing to make sure everything is written in the same order
	Concurrency::SingleMutex mutex;
#endif

	//the suffix to append to the file on close, if any
	std::string fileSuffix;
	//if true, will pretty print the logs
	bool pretty;
	//if true, will sort keys when printing
	bool sortKeys;
};
