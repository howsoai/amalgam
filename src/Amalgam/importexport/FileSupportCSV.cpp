//project headers:
#include "FileSupportCSV.h"

//system headers:
#include <iostream>
#include <string>

EvaluableNode *FileSupportCSV::Load(const std::string &resource_path, EvaluableNodeManager *enm, EntityExternalInterface::LoadEntityStatus &status)
{
	auto [data, data_success] = Platform_OpenFileAsString(resource_path);
	if(!data_success)
	{
		status.SetStatus(false, data);
		std::cerr << data << std::endl;
		return EvaluableNodeReference::Null();
	}

	StringManipulation::RemoveBOMFromUTF8String(data);
	size_t file_size = data.size();

	EvaluableNode *data_top_node = enm->AllocNode(ENT_LIST);

	//position in the input
	size_t cur_position = 0;

	//for each row
	while(cur_position < file_size)
	{
		EvaluableNode *cur_row = enm->AllocNode(ENT_LIST);
		auto &cur_row_ocn = cur_row->GetOrderedChildNodesReference();
		data_top_node->GetOrderedChildNodesReference().push_back(cur_row);

		//instantiate this once so the memory can be reused and start with a reasonable string size
		std::string value;
		value.reserve(64);

		//for each column
		while(cur_position < file_size)
		{
			//get this column's characters and value
			size_t end_position = cur_position;
			bool end_of_row = false;
			value.clear();

			//while in this column
			while(end_position < file_size)
			{
				//if quoted string, get the rest of the quote
				if(data[end_position] == '"')
				{
					//advance past quote
					cur_position++;
					end_position++;

					while(end_position < file_size)
					{
						//scan through everything that isn't a quote
						if(data[end_position] != '"')
						{
							value.push_back(data[end_position]);
							end_position++;
							continue;
						}

						//must be a quote, check for two in a row
						if(end_position + 1 < file_size && data[end_position + 1] == '"')
						{
							value.push_back('"');
							//skip both quotes
							end_position += 2;
							continue;
						}

						//must be a quote
						end_position++;
						break;
					}

					//catch up the the cur_position because these characters have already been accounted for
					cur_position = end_position;
				}

				if(data[end_position] == ',')
					break;

				if(data[end_position] == '\n' || data[end_position] == '\r')
				{
					end_of_row = true;
					break;
				}

				//keep accumulating this column
				end_position++;
			}

			//accumulate any remaining value
			value.append(std::string(&data[cur_position], &data[end_position]));

			//move past extra terminating character if applicable
			if(end_position + 1 < file_size && data[end_position] == '\r' && data[end_position + 1] == '\n')
				end_position++;
			//move past terminating character
			end_position++;

			//create the value
			EvaluableNode *element = nullptr;
			if(value.size() > 0)
			{
				auto [float_value, success] = Platform_StringToNumber(value);
				if(success)
					element = enm->AllocNode(float_value);
				else
					element = enm->AllocNode(ENT_STRING, value);
			}
			cur_row_ocn.push_back(element);

			//start at next field
			cur_position = end_position;

			if(end_of_row)
				break;
		}
	}

	return data_top_node;
}

//escapes a string per the CSV standard
// may return the original string
std::string EscapeCSVStringIfNeeded(std::string &s)
{
	if(		   s.find(',') == std::string::npos
			&& s.find('"') == std::string::npos
			&& s.find('\r') == std::string::npos
			&& s.find('\n') == std::string::npos)
		return s;

	//need to put quotes around it and escape characters
	std::string result;
	result.reserve(s.size() + 2);
	result.push_back('"');
	for(auto &c : s)
	{
		//quotes should be double quoted
		if(c == '"')
			result.push_back('"');
		result.push_back(c);
	}
	result.push_back('"');

	return result;
}

bool FileSupportCSV::Store(EvaluableNode *code, const std::string &resource_path, EvaluableNodeManager *enm)
{
	std::ofstream outf(resource_path, std::ios::out | std::ios::binary);
	if(!outf.good())
		return false;

	//data to write
	std::string data_string;

	if(code != nullptr)
	{
		//grab rows
		for(auto &row_node : code->GetOrderedChildNodes())
		{
			//if nothing, skip
			if(row_node == nullptr)
			{
				data_string.push_back('\n');
				continue;
			}

			bool is_first_column = true;
			for(auto &column_node : row_node->GetOrderedChildNodes())
			{
				//separate fields by commas
				if(!is_first_column)
					data_string.push_back(',');
				else //must be first column, but no longer
					is_first_column = false;

				//leave nulls blank
				if(EvaluableNode::IsNull(column_node))
					continue;

				std::string original_string = EvaluableNode::ToString(column_node);
				std::string escaped_str = EscapeCSVStringIfNeeded(original_string);
				data_string.append(escaped_str);
			}

			data_string.push_back('\n');
		}
	}

	outf.write(data_string.c_str(), data_string.size());
	outf.close();

	return true;
}
