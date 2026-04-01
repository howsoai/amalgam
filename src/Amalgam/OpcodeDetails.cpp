//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"

//system headers:
#include <regex>

EvaluableNode *ExecutionPermissions::GetPermissionsAsEvaluableNode(EvaluableNodeManager *enm)
{
	EvaluableNode *permissions_en = enm->AllocNode(ENT_ASSOC);
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err),
		enm->AllocNode(HasPermission(Permission::STD_OUT_AND_STD_ERR)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_in),
		enm->AllocNode(HasPermission(Permission::STD_IN)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_load),
		enm->AllocNode(HasPermission(Permission::LOAD)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_store),
		enm->AllocNode(HasPermission(Permission::STORE)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_environment),
		enm->AllocNode(HasPermission(Permission::ENVIRONMENT)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_alter_performance),
		enm->AllocNode(HasPermission(Permission::ALTER_PERFORMANCE)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_system),
		enm->AllocNode(HasPermission(Permission::SYSTEM)));

	return permissions_en;
}

std::pair<ExecutionPermissions, ExecutionPermissions> ExecutionPermissions::EvaluableNodeToPermissions(EvaluableNode *en)
{
	ExecutionPermissions permissions_to_set;
	ExecutionPermissions permission_values;

	if(EvaluableNode::IsAssociativeArray(en))
	{
		for(auto [permission_type, allow_en] : en->GetMappedChildNodesReference())
		{
			bool allow = EvaluableNode::ToBool(allow_en);

			if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err))
			{
				permissions_to_set.SetPermission(Permission::STD_OUT_AND_STD_ERR, true);
				permission_values.SetPermission(Permission::STD_OUT_AND_STD_ERR, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_in))
			{
				permissions_to_set.SetPermission(Permission::STD_IN, true);
				permission_values.SetPermission(Permission::STD_IN, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_load))
			{
				permissions_to_set.SetPermission(Permission::LOAD, true);
				permission_values.SetPermission(Permission::LOAD, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_store))
			{
				permissions_to_set.SetPermission(Permission::STORE, true);
				permission_values.SetPermission(Permission::STORE, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_environment))
			{
				permissions_to_set.SetPermission(Permission::ENVIRONMENT, true);
				permission_values.SetPermission(Permission::ENVIRONMENT, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_alter_performance))
			{
				permissions_to_set.SetPermission(Permission::ALTER_PERFORMANCE, true);
				permission_values.SetPermission(Permission::ALTER_PERFORMANCE, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_system))
			{
				permissions_to_set.SetPermission(Permission::SYSTEM, true);
				permission_values.SetPermission(Permission::SYSTEM, allow);
			}
		}
	}
	else if(EvaluableNode::ToBool(en))
	{
		permissions_to_set = AllPermissions();
		permission_values = AllPermissions();
	}

	return std::make_pair(permissions_to_set, permission_values);
}


//returns a copy of s where each consecutive whitespace block is replaced
//by a single space, any leading and trailing spaces are removed,
//numbers with large precision are truncated to remove errors of
//insignificant rounding across platforms, and unnamed entity ids are replaced with
//underscores since they are not reliable based on differences of randomization
//across platforms
static std::string NormalizeTestValidationString(std::string_view s)
{
	std::string out;
	out.reserve(s.size());

	bool in_whitespace = false;
	bool after_dot = false;
	int frac_count = 0;
	for(size_t i = 0; i < s.size(); i++)
	{
		char ch = s[i];

		if(std::isspace(static_cast<unsigned char>(ch)))
		{
			//if first whitespace, change to space
			if(!in_whitespace)
			{
				out.push_back(' ');
				in_whitespace = true;
			}
			continue;
		}

		in_whitespace = false;

		if(ch == '.' && i + 1 < s.size()
			&& std::isdigit(static_cast<unsigned char>(s[i + 1])))
		{
			after_dot = true;
			frac_count = 0;
			out.push_back(ch);
			continue;
		}

		//decimal digit truncation
		if(after_dot)
		{
			if(std::isdigit(static_cast<unsigned char>(ch)))
			{
				frac_count++;
				//keep only the first 6 digits after the decimal place
				if(frac_count <= 6)
					out.push_back(ch);

				continue;
			}
			else //any other character ends the number
			{
				after_dot = false;
				frac_count = 0;
			}
		}

		//detect a quoted string that starts with an underscore as an unnamed entity reference
		if(ch == '"' && i + 1 < s.size())
		{
			size_t closing_quote_pos = i + 1;
			while(closing_quote_pos < s.size() && s[closing_quote_pos] != '"')
				closing_quote_pos++;

			//if found closing quote, then normalize
			if(closing_quote_pos < s.size() && s[i + 1] == '_')
			{
				out.append("\"_____\"");
				i = closing_quote_pos;
				continue;
			}

			//otherwise normal character
		}

		out.push_back(ch);
	}

	//trim spaces
	if(!out.empty() && out.front() == ' ')
		out.erase(out.begin());
	if(!out.empty() && out.back() == ' ')
		out.pop_back();

	return out;
}

//returns true if a and b are equal ignoring subtle differences due to differing platforms
inline static bool EqualGivenValidationNormalization(std::string_view a, std::string_view b)
{
	return NormalizeTestValidationString(a) == NormalizeTestValidationString(b);
}

bool AmalgamExample::ValidateExample(Entity *entity)
{
	bool test_succeeded = true;
	std::cout << "Initializing... ";

	entity->SetRandomState("12345", true);

	auto [code, warnings, char_with_error, code_complete]
		= Parser::Parse(example, &entity->evaluableNodeManager);

	if(warnings.size() > 0)
	{
		std::cerr << "Improper code: " << std::endl;
		for(auto &w : warnings)
			std::cerr << w << std::endl;

		return false;
	}

	std::cout << "Executing... ";
	auto result = entity->ExecuteOnEntity(code, nullptr);
	std::string result_str = Parser::Unparse(result, true, true, true);

	if(regexMatch.empty())
	{
		if(!EqualGivenValidationNormalization(result_str, output))
		{
			std::cerr << "Failed, ran code:" << std::endl;
			std::cerr << example << std::endl;
			std::cerr << "Expected result:" << std::endl;
			std::cerr << output << std::endl;
			std::cerr << "Observed result:" << std::endl;
			std::cerr << result_str << std::endl;
			test_succeeded = false;
		}
	}
	else //match with regular expression
	{
		//use the begin/end constructor since std::string_view isn't universally supported
		std::regex pattern(begin(regexMatch), end(regexMatch), std::regex::ECMAScript);
		if(std::regex_match(result_str, pattern))
		{
			std::cerr << "Failed, ran code:" << std::endl;
			std::cerr << example << std::endl;
			std::cerr << "Expected to match:" << std::endl;
			std::cerr << regexMatch << std::endl;
			std::cerr << "Observed:" << std::endl;
			std::cerr << result_str << std::endl;
			test_succeeded = false;
		}
	}

	std::cout << "Reclaiming Resources... ";
	if(!cleanup.empty())
	{
		auto [cleanup_code, cleanup_warnings, cleanup_char_with_error, cleanup_code_complete]
			= Parser::Parse(cleanup, &entity->evaluableNodeManager);

		entity->ExecuteOnEntity(cleanup_code, nullptr);
	}

	entity->ReclaimResources(false, true, false);

	auto query_caches = entity->GetQueryCaches();
	if(query_caches != nullptr)
		query_caches->sbfds.VerifyAllEntitiesForAllColumns();

	if(entity->GetLabelIndex().size() != 0)
	{
		std::cerr << "Failed: Labels remain in entity after test" << std::endl;
		test_succeeded = false;
	}

	if(entity->GetContainedEntities().size() > 0)
	{
		std::cerr << "Failed: One or more contained entities remain after test" << std::endl;
		test_succeeded = false;
	}

	return test_succeeded;
}

UninitializedArray<OpcodeDetails, NUM_ENT_OPCODES> _opcode_details;

static std::string _opcode_group = "global";

static OpcodeInitializer _ENT_RAND(ENT_RAND, &Interpreter::InterpretNode_ENT_RAND, []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|number range] [number number_to_generate] [bool unique])";
		d.returns = R"(any)";
		d.description = R"(Generates random values based on its parameters.  The random values are drawn from a random stream specific to each execution flow for each entity.  With no range, evaluates to a random number between 0.0 and 1.0.  If range is a list, it will uniformly randomly choose and evaluate to one element of the list.  If range is a number, it will evaluate to a value greater than or equal to zero and less than the number specified.  If range is an assoc, then it will randomly evaluate to one of the keys using the values as the weights for the probabilities.  If  number_to_generate is specified, it will generate a list of multiple values (even if number_to_generate is 1).  If unique is true (it defaults to false), then it will only return unique values, the same as selecting from the list or assoc without replacement.  Note that if unique only applies to list and assoc ranges.  If unique is true and there are not enough values in a list or assoc, it will only generate the number of elements in range.)";
		d.examples = MakeAmalgamExamples({
			{R"&((rand))&", R"(0.4153759082605256)"},
			{R"&((rand 50))&", R"(20.768795413026282)"},
			{R"&((rand
	[1 2 4 5 7]
))&", R"(1)"},
			{R"&((rand
	(range 0 10)
))&", R"(4)"},
			{R"&((rand
	(range 0 10)
	0
))&", R"([])"},
			{R"&((rand
	(range 0 10)
	1
))&", R"([4])"},
			{R"&((rand
	(range 0 10)
	10
	.true
))&", R"([
	4
	0
	5
	9
	10
	1
	2
	7
	6
	8
])"},
			{R"&((rand 50 4))&", R"([20.768795413026282 23.51742714184096 6.034392211178502 29.777315548569128])"},
			{R"&((rand
	(associate "a" 0.25 "b" 0.75)
))&", R"("b")"},
			{R"&((rand
	(associate "a" 0.25 "b" 0.75)
	16
))&", "", R"&(\[\s*
    "(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
\])&"
},
			{R"&((rand
	(associate
		"a"
		0.25
		"b"
		0.75
		"c"
		.infinity
		"d"
		.infinity
	)
	4
))&", R"(["c" "c" "c" "d"])",
R"&(\[\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
\])&"
},
			{R"&(;should come out somewhere near the correct proportion
(zip
	(lambda
		(+
			(current_value 1)
			(current_value)
		)
	)
	(rand
		(associate "a" 0.25 "b" 0.5 "c" 0.25)
		100
	)
	1
))&", R"({a 30 b 50 c 20})",
			R"&(\{\s*
    a\s+(\d+)\s+
    b\s+(\d+)\s+
    c\s+(\d+)\s*
\})&"
},
			{R"&(;these should be weighted toward smaller numbers
(rand
	(zip
		(range 1 10)
		(map
			(lambda
				(/
					(/ 1 (current_value))
					2
				)
			)
			(range 1 10)
		)
	)
	3
	.true
))&", R"([2 6 1])",
			R"&(\[\s*
    (\d+)\s*
    (\d+)\s*
    (\d+)\s*
\])&"
}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		d.hasSideEffects = true;
		d.frequencyPer10000Opcodes = 6.5;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_GET_RAND_SEED(ENT_GET_RAND_SEED, &Interpreter::InterpretNode_ENT_GET_RAND_SEED, []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(string)";
		d.description = R"(Evaluates to a string representing the current state of the random number generator.  Note that the string will be a string of bytes that may not be valid as UTF-8.)";
		d.examples = MakeAmalgamExamples({
			{R"&((format (get_rand_seed) "string" "base64"))&", R"("X6f8e5JTT5kuHHGZUu7r6/8=")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.frequencyPer10000Opcodes = 0.25;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_SET_RAND_SEED(ENT_SET_RAND_SEED, &Interpreter::InterpretNode_ENT_SET_RAND_SEED, []() {
		OpcodeDetails d;
		d.parameters = R"(string seed)";
		d.returns = R"(string)";
		d.description = R"(Initializes the random number stream for the given `seed` without affecting any entity.  If the seed is already a string in the proper format output by `get_entity_rand_seed` or `get_rand_seed`, then it will set the random generator to that current state, picking up where the previous state left off.  If it is anything else, it uses the value as a random seed to start the generator.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		{cur_seed (get_rand_seed)}
	)
	(declare
		{
			first_pair [(rand) (rand)]
		}
	)
	(set_rand_seed cur_seed)
	(declare
		{
			second_pair [(rand) (rand)]
		}
	)
	(append first_pair second_pair)
))&", R"([0.4153759082605256 0.47034854283681926 0.4153759082605256 0.47034854283681926])"},
			{R"&((seq
	(set_rand_seed "12345")
	(rand)
))&", R"(0.5507987428849511)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.hasSideEffects = true;
		d.frequencyPer10000Opcodes = 0.5;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_CRYPTO_SIGN(ENT_CRYPTO_SIGN, &Interpreter::InterpretNode_ENT_CRYPTO_SIGN, []() {
		OpcodeDetails d;
		d.parameters = R"(string message string secret_key)";
		d.returns = R"(string)";
		d.description = R"(Signs `message` given `secret_key` and returns the signature using the Ed25519 algorithm.  Note that `message` is not included in the `signature`.  The `system` opcode using the command "sign_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_sign_key" "secret_sign_key"]
			(system "sign_key_pair")
		)
	)
	(declare
		{message "hello"}
	)
	(declare
		{
			signature (crypto_sign message secret_sign_key)
		}
	)
	(concat
		"valid signature: "
		(crypto_sign_verify message public_sign_key signature)
		"\n"
	)
))&", R"("valid signature: .true\n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.frequencyPer10000Opcodes = 0.01;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_CRYPTO_SIGN_VERIFY(ENT_CRYPTO_SIGN_VERIFY, &Interpreter::InterpretNode_ENT_CRYPTO_SIGN_VERIFY, []() {
		OpcodeDetails d;
		d.parameters = R"(string message string public_key string signature)";
		d.returns = R"(bool)";
		d.description = R"(Verifies that `message` was signed with the signature via the public key using the Ed25519 algorithm and returns true if the signature is valid, false otherwise.  Note that `message` is not included in the `signature`.  The `system` opcode using the command "sign_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_sign_key" "secret_sign_key"]
			(system "sign_key_pair")
		)
	)
	(declare
		{message "hello"}
	)
	(declare
		{
			signature (crypto_sign message secret_sign_key)
		}
	)
	(concat
		"valid signature: "
		(crypto_sign_verify message public_sign_key signature)
		"\n"
	)
))&", R"("valid signature: .true\n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.frequencyPer10000Opcodes = 0.01;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_ENCRYPT(ENT_ENCRYPT, &Interpreter::InterpretNode_ENT_ENCRYPT, []() {
		OpcodeDetails d;
		d.parameters = R"(string plaintext_message string key1 [string nonce] [string key2])";
		d.returns = R"(string)";
		d.description = R"(If `key2` is not provided, then it uses the XSalsa20 algorithm to perform shared secret key encryption on the `message`, returning the encrypted value.  If `key2` is provided, then the Curve25519 algorithm will additionally be used, and `key1` will represent the receiver's public key and `key2` will represent the sender's secret key.  The `nonce` is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The `system` opcode using the command "encrypt_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_encrypt_key" "secret_encrypt_key"]
			(system "encrypt_key_pair")
		)
	)
	(declare
		{
			encrypted (encrypt message secret_encrypt_key "1234")
		}
	)
	(concat
		"decrypted: "
		(decrypt encrypted secret_encrypt_key "1234")
		"\n"
	)
))&", R"("decrypted: \n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.frequencyPer10000Opcodes = 0.01;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_DECRYPT(ENT_DECRYPT, &Interpreter::InterpretNode_ENT_DECRYPT, []() {
		OpcodeDetails d;
		d.parameters = R"(string cyphertext_message string key1 [string nonce] [string key2])";
		d.returns = R"(string)";
		d.description = R"(If `key2` is not provided, then it uses the XSalsa20 algorithm to perform shared secret key decryption on the `message`, returning the encrypted value.  If `key2` is provided, then the Curve25519 algorithm will additionally be used, and `key1` will represent the sender's public key and `key2` will represent the receiver's secret key.  The `nonce` is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The `system` opcode using the command "encrypt_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_encrypt_key" "secret_encrypt_key"]
			(system "encrypt_key_pair")
		)
	)
	(declare
		{
			encrypted (encrypt message secret_encrypt_key "1234")
		}
	)
	(concat
		"decrypted: "
		(decrypt encrypted secret_encrypt_key "1234")
		"\n"
	)
))&", R"("decrypted: \n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.frequencyPer10000Opcodes = 0.01;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_GET_ENTITY_RAND_SEED(ENT_GET_ENTITY_RAND_SEED, &Interpreter::InterpretNode_ENT_GET_ENTITY_RAND_SEED, []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity])";
		d.returns = R"(string)";
		d.description = R"(Evaluates to a string representing the current state of the random number generator for `entity` used for seeding the random streams of any calls to the entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Rand"
		(lambda
			{a (rand)}
		)
	)
	(call_entity "Rand" "a")
	(format
		(get_entity_rand_seed "Rand")
		"string"
		"base64"
	)
))&", R"("nHKVcHddHVaqvcDt3AYbD/8=")", "", R"((destroy_entities "Rand"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.frequencyPer10000Opcodes = 0.01;
		d.opcodeGroup = _opcode_group;
		return d;
	});

static OpcodeInitializer _ENT_SET_ENTITY_RAND_SEED(ENT_SET_ENTITY_RAND_SEED, &Interpreter::InterpretNode_ENT_SET_ENTITY_RAND_SEED, []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity] * node [bool deep])";
		d.returns = R"(string)";
		d.description = R"(Sets the random number seed and state for the random number generator of `entity`, or the current entity if null or not specified, to the state specified by `node`.  If `node` is already a string in the proper format output by `(get_entity_rand_seed)`, then it will set the random generator to that current state, picking up where the previous state left off.  If `node` is anything else, it uses the value as a random seed to start the generator.  Note that this will not affect the state of the current random number stream, only future random streams created by `entity` for new calls.  The parameter `deep` defaults to false, but if it is true, all contained entities are recursively set with random seeds based on the specified random seed and a hash of their relative id path to the entity being set.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Rand"
		(lambda
			{a (rand)}
		)
	)
	(create_entities
		["Rand" "DeepRand"]
		(lambda
			{a (rand)}
		)
	)
	(declare
		{
			seed (get_entity_rand_seed "Rand")
		}
	)
	(declare
		{
			first_rand_numbers [
					(call_entity "Rand" "a")
					(call_entity
						["Rand" "DeepRand"]
						"a"
					)
				]
		}
	)
	(set_entity_rand_seed "Rand" seed .true)
	(declare
		{
			second_rand_numbers [
					(call_entity "Rand" "a")
					(call_entity
						["Rand" "DeepRand"]
						"a"
					)
				]
		}
	)
	[first_rand_numbers second_rand_numbers]
))&", R"([
	[0.9512993766655248 0.3733350484591008]
	[0.9512993766655248 0.3733350484591008]
])", "", R"((destroy_entities "Rand"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.hasSideEffects = true;
		d.frequencyPer10000Opcodes = 0.5;
		d.opcodeGroup = _opcode_group;
		return d;
	});
