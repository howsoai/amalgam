#pragma once

//project headers:
#include "StringInternPool.h"

//syatem headers:
#include <string>

//forward declarations:
class Entity;
class EvaluableNode;

class EntityWriteCallbacks
{
public:
	virtual ~EntityWriteCallbacks();

	virtual void LogSystemCall(EvaluableNode *params) = 0;

	// LogPrint does not flush to allow bulk processing
	virtual void LogPrint(std::string &print_string) = 0;

	virtual void LogWriteValueToEntity(Entity *entity, EvaluableNode *value, const StringInternPool::StringID label_name, bool direct_set) = 0;
	
	//like LogWriteValueToEntity but where the keys are the labels and the values correspond in the assoc specified by label_value_pairs
	virtual void LogWriteValuesToEntity(Entity *entity, EvaluableNode *label_value_pairs, bool direct_set) = 0;

	virtual void LogWriteToEntity(Entity *entity, const std::string &new_code) = 0;

	virtual void LogCreateEntity(Entity *new_entity) = 0;

	virtual void LogDestroyEntity(Entity *destroyed_entity) = 0;

	virtual void LogSetEntityRandomSeed(Entity *entity, const std::string &rand_seed, bool deep_set) = 0;

	virtual void FlushLogFile() = 0;

	//returns all writes that the listener was aware of
	virtual EvaluableNode *GetWrites() = 0;
};
