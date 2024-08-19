//project headers:
#include "EntityExecution.h"
#include "Entity.h"
#include "Interpreter.h"

void EntityExecution::ExecuteEntity(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	bundle->entity->Execute(label, nullptr, false, nullptr, &bundle->writeListeners, bundle->printListener);
}
