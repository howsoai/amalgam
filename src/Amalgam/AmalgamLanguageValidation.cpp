//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "Parser.h"
#include "PerformanceProfiler.h"
#include "PlatformSpecific.h"

template<class... Ts>
constexpr std::array<AmalgamExample, sizeof...(Ts)>
MakeAmalgamUnitTests(Ts... elems)
{
	return { std::forward<Ts>(elems)... };
}

auto _amalgam_unit_tests = MakeAmalgamUnitTests(
	AmalgamExample{ R"&((associate "a" 1 "b" 2))&", R"({a 1 b 2})" },
	AmalgamExample{ R"&((lambda
	(associate "a" 1 "b" 2)
))&", R"((associate "a" 1 "b" 2))" },
AmalgamExample{ R"&((get
	(lambda
		{a 1 b 2}
	)
	0
))&", R"((null))" },
AmalgamExample{ R"&((get
	(lambda
		{a 1 b 2}
	)
	"a"
))&", R"(1)" },
AmalgamExample{ R"&((get
	(lambda
		["a" 1 "b" 2]
	)
	"a"
))&", R"((null))" },
AmalgamExample{ R"&((replace
	[
		(associate "a" 1)
	]
	[2]
	1
	[1]
	(lambda
		(get (target) 0)
	)
))&", R"([
	{a 1}
	@(target .true 0)
	1
])" },
AmalgamExample{ R"&([
	{a 1}
	@(target .true 0)
	1
])&", R"([
	{a 1}
	@(target .true 0)
	1
])" },
AmalgamExample{ R"&([
	{a 1}
	@(target .true 0)
	1
])&", R"([
	{a 1}
	@(target .true 0)
	1
])" },
AmalgamExample{ R"&([
	{a 1}
	{
		b @(target .true 0)
	}
	1
])&", R"([
	{a 1}
	{
		b @(target .true 0)
	}
	1
])" },
AmalgamExample{ R"&([
	0
	1
	2
	3
	(+
		(target 0 1)
	)
	4
])&", R"([0 1 2 3 1 4])" },
AmalgamExample{ R"&([
	0
	1
	2
	3
	[
		0
		1
		2
		3
		(+
			(target 1 1)
		)
		4
	]
])&", R"([
	0
	1
	2
	3
	[0 1 2 3 1 4]
])" },
AmalgamExample{ R"&({
	a 0
	b 1
	c 2
	d 3
	e [
			0
			1
			2
			3
			(+
				(target 1 "a")
			)
			4
		]
})&", R"({
	a 0
	b 1
	c 2
	d 3
	e [0 1 2 3 0 4]
})" },
AmalgamExample{ R"&({
	a 0
	b 1
	c 2
	d 3
	e [
			[
				0
				1
				2
				3
				(+
					(target .true "a")
				)
				4
			]
		]
})&", R"({
	a 0
	b 1
	c 2
	d 3
	e [
			[0 1 2 3 0 4]
		]
})" },
AmalgamExample{ R"&([
	{a 3}
	
	;@(get (target 2) [0 "a"])
	(get
		(target)
		[0 "a"]
	)
])&", R"([
	{a 3}
	@(target
		.true
		[0 "a"]
	)
])" },
AmalgamExample{ R"&((seq
	(create_entities
		"test"
		{
			a {a 3}
			b @(target 1 "a")
		}
	)
	(call
		(flatten_entity "test")
		{new_entity "test2"}
	)
	(flatten_entity "test2")
))&", R"((declare
	{create_new_entity .true new_entity (null) require_version_compatibility .false}
	(let
		{
			_ (lambda
					{
						a {a 3}
						b @(target 1 "a")
					}
				)
		}
		(if
			create_new_entity
			(assign
				"new_entity"
				(first
					(create_entities new_entity _)
				)
			)
			(assign_entity_roots new_entity _)
		)
	)
	(set_entity_rand_seed new_entity "›è©ìÝ¦Iç/…çS•ÿ")
	new_entity
))", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((seq
	(declare
		{id "id"}
	)
	(declare
		{
			data_assoc {
					original {
							auto_derive_on_train {series_id_features id}
						}
					pointer [
							[id]
						]
				}
		}
	)
	(declare
		{
			mymap {
					".f1_rate_1" {
							auto_derive_on_train {
									ordered_by_features ["date"]
								}
						}
					".f3_rate_1" {
							auto_derive_on_train {
									ordered_by_features [@(target 4 [".f1_rate_1" "auto_derive_on_train" "ordered_by_features" 0])]
								}
						}
				}
		}
	)
	mymap
))&", R"({
	".f1_rate_1" {
			auto_derive_on_train {
					ordered_by_features ["date"]
				}
		}
	".f3_rate_1" {
			auto_derive_on_train {
					ordered_by_features [@(target .true [".f1_rate_1" "auto_derive_on_train" "ordered_by_features" 0])]
				}
		}
})" },
AmalgamExample{ R"&((= (null) (null)))&", R"(.true)" },
AmalgamExample{ R"&((=
	(+ (null))
	(null)
))&", R"(.true)" },
AmalgamExample{ R"&((seq
	(create_entities
		["NaNTest"]
		(null)
	)
	(create_entities
		["NaNTest" "Entity3"]
		{label1 3 label2 1}
	)
	(create_entities
		["NaNTest" "EntityNull"]
		{label1 (null) label2 1}
	)
	(create_entities
		["NaNTest" "EntityNaN"]
		{label1 (null) label2 1}
	)
	[
		(contained_entities
			"NaNTest"
			[
				(query_equals "label1" 3)
				(query_exists "label2")
			]
		)
		(contained_entities
			"NaNTest"
			[
				(query_equals "label1" (null))
				(query_exists "label2")
			]
		)
		(contained_entities
			"NaNTest"
			[
				(query_equals "label1" (null))
				(query_exists "label2")
			]
		)
		(contained_entities
			"NaNTest"
			[
				(query_nearest_generalized_distance
					3
					["label1"]
					1
					[0]
				)
			]
		)
	]
))&", R"([
	["Entity3"]
	["EntityNull" "EntityNaN"]
	["EntityNull" "EntityNaN"]
	["Entity3" "EntityNull" "EntityNaN"]
])", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((call
	(set_type
		[1 0.5 "3"]
		(get_comments
			(lambda
				
				;+
				(null)
			)
		)
	)
))&", R"(4.5)" },
AmalgamExample{ R"&(;compute distance between two vectors
(pow
	(reduce
		(lambda
			(+ (previous_result) (current_value))
		)
		(map
			(lambda
				(pow
					(-
						(get (current_value) 0)
						(get (current_value) 1)
					)
					2
				)
			)
			[3 4]
			[0 0]
		)
	)
	0.5
))&", R"(5)" },
AmalgamExample{ R"&((seq
	(create_entities "combo_query" (null))
	(create_entities
		["combo_query" "world"]
		{
			A 6
			B 7
			C 9
			D 1
		}
	)
	(create_entities
		["combo_query" "hello"]
		{
			A 6
			B 7
			C 19
			D 1
			E 2
			F 3
		}
	)
	(create_entities
		["combo_query" "!"]
		{
			A 6
			B 7
			C 19
			D 1
			E 2
			F 3
		}
	)
	(contained_entities
		"combo_query"
		[
			(query_exists "A")
			(query_greater_or_equal_to "B" 5)
			(query_exists "B")
			(query_greater_or_equal_to "C" 18)
		]
	)
))&", R"(["hello" "!"])", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((seq
	(create_entities "nan_queries" (null))
	(create_entities
		["nan_queries" "a1"]
		{A 10 B (null)}
	)
	(create_entities
		["nan_queries" "a2"]
		{A 11 B 2}
	)
	(create_entities
		["nan_queries" "a3"]
		{A (null) B 1}
	)
	[
		
		;expected output is 3 neighbors in order: a1, a2, a3
		(compute_on_contained_entities
			"nan_queries"
			[
				(query_nearest_generalized_distance
					3
					
					;k-value
					["A" "B"]
					[9 2]
					2
				)
			]
		)
		
		;expected output is 2 neighbors in order: a1, a2
		(compute_on_contained_entities
			"nan_queries"
			[
				(query_nearest_generalized_distance
					2
					
					;k-value
					["A" "B"]
					[9 2]
					2
				)
			]
		)
		
		;expected output is only 1 neighbor, a1 or a3
		(compute_on_contained_entities
			"nan_queries"
			[
				(query_nearest_generalized_distance
					1
					
					;k-value
					["A" "B"]
					[9 2]
					2
				)
			]
		)
		
		;expected output is 3 neighbors in order: a2, a1/a3
		(compute_on_contained_entities
			"nan_queries"
			[
				(query_nearest_generalized_distance
					3
					
					;k-value
					["A" "B"]
					
					;labels
					[9 2]
					
					;values
					2
					
					;p-value
					(null)
					
					;weights
					["continuous_number" "continuous_number"]
					
					;distance types
					(null)
					
					;attributes
					[
						[0 5 6]
						[0 5 5]
					]
				)
			]
		)
		(create_entities
			["nan_queries" "a4"]
			{A (null) B (null)}
		)
		
		;expected output is 3 neighbors in order: a4, a1/a3
		(compute_on_contained_entities
			"nan_queries"
			[
				(query_nearest_generalized_distance
					3
					
					;k-value
					["A" "B"]
					
					;labels
					[(null) (null)]
					
					;values
					2
					
					;p-value
					(null)
					
					;weights
					["continuous_number" "continuous_number"]
					
					;distance types
					(null)
					
					;attributes
					[
						[0 1 0]
						[0 1 0]
					]
				)
			]
		)
	]
))&", R"([
	{a1 1.4142135623730951 a2 2 a3 1.4142135623730951}
	{a1 1.4142135623730951 a3 1.4142135623730951}
	{a3 1.4142135623730951}
	{a1 5.0990195135927845 a2 2 a3 5.0990195135927845}
	[
		["nan_queries" "a4"]
	]
	{a1 1 a3 1 a4 0}
])", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((seq
	(create_entities "bool_test" (null))
	(create_entities
		["bool_test" "case1"]
		(zip
			["A" "B"]
			[.true 1]
		)
	)
	(create_entities
		["bool_test" "case2"]
		(zip
			["A" "B"]
			[.true 2]
		)
	)
	(create_entities
		["bool_test" "case3"]
		(zip
			["A" "B"]
			[.false 1]
		)
	)
	[
		(contained_entities
			"bool_test"
			(query_equals "A" .true)
		)
		(compute_on_contained_entities
			"bool_test"
			(query_mode "A" (null))
		)
	]
))&", R"([
	["case1" "case2"]
	.true
])", "", R"((apply "destroy_entities" (contained_entities)))" },
AmalgamExample{ R"&((generalized_distance
	[1 2 1 100 1 50]
	[1 1 1 120 1 50.1]
	
	;p
	0.5
	
	;weights
	[2.5 10 5 0.033333333 3.333333333 20]
	
	;types
	["nominal_number" "nominal_number" "nominal_number" "continuous_number" "nominal_number" "continuous_number"]
	
	;attributes
	[3 100 7 (null) 10 (null)]
	
	;deviations
	[0.4 0.1 0.2 30 0.3 0.05]
))&", R"(1036.1581794564518)" },
AmalgamExample{ R"&((generalized_distance
	
	;point 1
	[1 2 1 100 1 50]
	
	;point 2
	[1 1 1 120 1 50.1]
	
	;p
	1
	
	;weights
	[2.5 10 5 0.033333333 3.333333333 20]
	
	;types
	["nominal_number" "nominal_number" "nominal_number" "continuous_number" "nominal_number" "continuous_number"]
	
	;attributes
	[3 100 7 (null) 10 (null)]
	
	;deviations
	[0.4 0.1 0.2 30 0.3 0.05]
))&", R"(24.769501899470985)" },
AmalgamExample{ R"&((generalized_distance
	
	;point 1
	[1 2 1 100 1 50]
	
	;point 2
	[1 1 1 120 1 50.1]
	
	;p
	0
	
	;weights
	[2.5 10 5 0.033333333 3.333333333 20]
	
	;types
	["nominal_number" "nominal_number" "nominal_number" "continuous_number" "nominal_number" "continuous_number"]
	
	;attributes
	[3 100 7 (null) 10 (null)]
	
	;deviations
	[0.4 0.1 0.2 30 0.3 0.05]
))&", R"(4.3464184709105573e-45)" },
AmalgamExample{ R"&((seq
	(create_entities
		"DistanceTestContainer"
		(lambda (null))
	)
	(create_entities
		["DistanceTestContainer" "point1"]
		{
			a 1
			b 1
			c 1
			d 120
			e 1
			f 50.1
		}
	)
	(create_entities
		["DistanceTestContainer" "point2"]
		{
			a 2
			b 1
			c 1
			d 120
			e 1
			f 50.1
		}
	)
	(create_entities
		["DistanceTestContainer" "point3"]
		{
			a 1
			b 1
			c 1
			d 120
			e 1
			f 50.1
		}
	)
	(create_entities
		["DistanceTestContainer" "point4"]
		{
			a 2
			b 1
			c 2
			d 160
			e 2
			f 51
		}
	)
	(create_entities
		["DistanceTestContainer" "point5"]
		{
			a 1
			b 1
			c 1
			d 119
			e 1
			f 49.8
		}
	)
	(map
		(lambda
			(create_entities
				["DistanceTestContainer"]
				{
					a 1
					b 1
					c 1
					d 119
					e 1
					f 49.8
				}
			)
		)
		(range 0 1000)
	)
	(compute_on_contained_entities
		"DistanceTestContainer"
		[
			(query_exists "a")
			(query_nearest_generalized_distance
				
				;k
				3
				
				;features
				["a" "b" "c" "d" "e" "f"]
				
				;center values
				[1 2 1 100 1 50]
				
				;p
				0.5
				
				;weights
				[2.5 10 5 0.033333333 3.333333333 20]
				
				;types
				["nominal_number" "nominal_number" "nominal_number" "continuous_number" "nominal_number" "continuous_number"]
				
				;attributes
				[3 100 7 (null) 10 (null)]
				
				;deviations
				[0.4 0.1 0.2 30 0.3 0.05]
				(null)
				
				;dwe
				1
				(null)
				
				; weight feature
				"random seed 1234"
			)
		]
	)
))&", R"({point1 1036.1581794564518 point2 978.5569789822398 point3 1036.1581794564518})", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((seq
	(create_entities "DistanceSymmetryContainer" (null))
	(create_entities
		["DistanceSymmetryContainer" "A"]
		{A 4 B 8}
	)
	(create_entities
		["DistanceSymmetryContainer" "B"]
		{A 4 B 9}
	)
	(create_entities
		["DistanceSymmetryContainer" "C"]
		{A 4 B 7}
	)
	(create_entities
		["DistanceSymmetryContainer" "D"]
		{A 4 B 10}
	)
	(create_entities
		["DistanceSymmetryContainer" "E"]
		{A 7 B 8}
	)
	(create_entities
		["DistanceSymmetryContainer" "F"]
		{A 7 B 9}
	)
	(create_entities
		["DistanceSymmetryContainer" "G"]
		{A 7 B 7}
	)
	(create_entities
		["DistanceSymmetryContainer" "H"]
		{A 10 B 8}
	)
	(create_entities
		["DistanceSymmetryContainer" "I"]
		{A 10 B 9}
	)
	(create_entities
		["DistanceSymmetryContainer" "J"]
		{A 10 B 10}
	)
	[
		(compute_on_contained_entities
			"DistanceSymmetryContainer"
			[
				(query_nearest_generalized_distance
					8
					
					; k
					["A" "B"]
					[4 9]
					0.1
					
					; p_parameter
					(null)
					
					; context_weights
					["nominal_number" "nominal_number"]
					
					; types
					[1 1]
					
					; attributes
					(null)
					
					; context_deviations
					(null)
					1
					
					; dwe = 1 means return computed distance to each case
					(null)
					
					; weight
					(rand)
					(null)
					"precise"
					.true
				)
			]
		)
		(compute_on_contained_entities
			"DistanceSymmetryContainer"
			[
				(query_nearest_generalized_distance
					8
					
					; k
					["B" "A"]
					[9 4]
					0.1
					
					; p_parameter
					(null)
					
					; context_weights
					["nominal_number" "nominal_number"]
					
					; types
					[1 1]
					
					; attributes
					(null)
					
					; context_deviations
					(null)
					1
					
					; dwe = 1 means return computed distance to each case
					(null)
					
					; weight
					(rand)
					(null)
					"precise"
					.true
				)
			]
		)
	]
))&", R"([
	[
		[
			"B"
			"D"
			"F"
			"A"
			"I"
			"C"
			"G"
			"H"
		]
		[
			0
			1
			1
			1
			1
			1
			1024
			1024
		]
	]
	[
		[
			"B"
			"F"
			"C"
			"D"
			"I"
			"A"
			"J"
			"E"
		]
		[
			0
			1
			1
			1
			1
			1
			1024
			1024
		]
	]
])", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((seq
	(create_entities "BoxConvictionTestContainer" (null))
	(create_entities
		["BoxConvictionTestContainer" "vert0"]
		(lambda
			{weight 2 x 0 y 0}
		)
	)
	(create_entities
		["BoxConvictionTestContainer" "vert1"]
		(lambda
			{weight 1 x 0 y 1}
		)
	)
	(create_entities
		["BoxConvictionTestContainer" "vert2"]
		(lambda
			{weight 1 x 1 y 0}
		)
	)
	(create_entities
		["BoxConvictionTestContainer" "vert3"]
		(lambda
			{weight 1 x 2 y 1}
		)
	)
	[
		(concat
			"dc: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_distance_contributions
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"weighted dc: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_distance_contributions
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
					)
				]
			)
		)
		"surprisal contributions"
		(concat
			"dc: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_distance_contributions
						2
						["x" "y"]
						(null)
						3
						(null)
						(null)
						(null)
						[0.25 0.25]
						(null)
						"surprisal"
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"weighted surprisal contributions: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_distance_contributions
						2
						["x" "y"]
						(null)
						3
						(null)
						(null)
						(null)
						[0.25 0.25]
						(null)
						"surprisal"
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
					)
				]
			)
		)
		(concat "removal conviction\n")
		(concat
			"kl: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_kl_divergences
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"weighted kl: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_kl_divergences
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"convictions: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_convictions
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"further parameterized convictions: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_convictions
						1
						["x" "y"]
						["vert0" "vert1" "vert2" "vert3"]
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"weighted convictions: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_convictions
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"group kl divergence: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_group_kl_divergence
						1
						["x" "y"]
						["vert1"]
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat
			"weighted group kl divergence: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_group_kl_divergence
						1
						["x" "y"]
						["vert1"]
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
						.true
					)
				]
			)
		)
		(concat "addition conviction\n")
		(concat
			"kl: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_kl_divergences
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.false
					)
				]
			)
		)
		(concat
			"weighted kl: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_kl_divergences
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
						.false
					)
				]
			)
		)
		(concat
			"convictions: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_convictions
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.false
					)
				]
			)
		)
		(concat
			"further parameterized convictions: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_convictions
						1
						["x" "y"]
						["vert0" "vert1" "vert2" "vert3"]
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.false
					)
				]
			)
		)
		(concat
			"weighted convictions: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_convictions
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
						.false
					)
				]
			)
		)
		(concat
			"group kl divergence: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_group_kl_divergence
						1
						["x" "y"]
						["vert1"]
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
						.false
					)
				]
			)
		)
		(concat
			"weighted group kl divergence: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_group_kl_divergence
						1
						["x" "y"]
						["vert1"]
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						"weight"
						"fixed_seed"
						(null)
						"recompute_precise"
						.false
					)
				]
			)
		)
		(concat "adding a case\n")
		(create_entities
			["BoxConvictionTestContainer" "vert4"]
			(lambda
				{x 3 y 0}
			)
		)
		(concat
			"noncyclic KL: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_kl_divergences
						1
						["x" "y"]
						(null)
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
					)
				]
			)
		)
		(concat
			"noncyclic group kl divergence: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_group_kl_divergence
						1
						["x" "y"]
						["vert4"]
						2
						(null)
						(null)
						(null)
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
					)
				]
			)
		)
		(concat
			"cyclic KL: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_kl_divergences
						1
						["x" "y"]
						(null)
						2
						(null)
						["continuous_number_cyclic" "continuous_number"]
						[3.5 (null)]
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
					)
				]
			)
		)
		(concat
			"cyclic conviction: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_convictions
						1
						["x" "y"]
						(null)
						2
						(null)
						["continuous_number_cyclic" "continuous_number"]
						[3.5 (null)]
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
					)
				]
			)
		)
		(concat
			"cyclic group kl divergence: "
			(compute_on_contained_entities
				"BoxConvictionTestContainer"
				[
					(query_entity_group_kl_divergence
						1
						["x" "y"]
						["vert4"]
						2
						(null)
						["continuous_number_cyclic" "continuous_number"]
						[3.5 (null)]
						(null)
						(null)
						-1
						(null)
						"fixed_seed"
						(null)
						"recompute_precise"
					)
				]
			)
		)
	]
))&", R"([
	"dc: 4"
	"weighted dc: 4"
	"surprisal contributions"
	"dc: 4"
	"weighted surprisal contributions: 4"
	"removal conviction\n"
	"kl: 4"
	"weighted kl: 4"
	"convictions: 4"
	"further parameterized convictions: 4"
	"weighted convictions: 4"
	"group kl divergence: 0.0014999155151563318"
	"weighted group kl divergence: 0.012150145528158986"
	"addition conviction\n"
	"kl: 4"
	"weighted kl: 4"
	"convictions: 4"
	"further parameterized convictions: 4"
	"weighted convictions: 4"
	"group kl divergence: 0.0015341620278852813"
	"weighted group kl divergence: 0.013070224898692494"
	"adding a case\n"
	[
		["BoxConvictionTestContainer" "vert4"]
	]
	"noncyclic KL: 5"
	"noncyclic group kl divergence: 0.005166280747839411"
	"cyclic KL: 5"
	"cyclic conviction: 5"
	"cyclic group kl divergence: 0.06081391029364306"
])", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((seq
	(create_entities "SurprisalTransformContainer" (null))
	(create_entities
		["SurprisalTransformContainer" "vert0"]
		(lambda
			{weight 2 x 3}
		)
	)
	(create_entities
		["SurprisalTransformContainer" "vert1"]
		(lambda
			{weight 0 x 3}
		)
	)
	(create_entities
		["SurprisalTransformContainer" "vert2"]
		(lambda
			{weight 1 x 4}
		)
	)
	(create_entities
		["SurprisalTransformContainer" "vert3"]
		(lambda
			{weight 1 x 5}
		)
	)
	[
		
		;(list "vert0" "vert1" "vert2" "vert3")
		(concat
			"probabilities: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_nearest_generalized_distance
						4
						
						; k
						["x"]
						[0]
						1
						
						; p_parameter
						(null)
						
						; context_weights
						["continuous_number"]
						
						; types
						(null)
						
						; attributes
						[0.25]
						
						; context_deviations
						(null)
						"surprisal_to_prob"
						
						; distance transform
						(null)
						
						; weight
						(rand)
						(null)
						"precise"
						.true
					)
				]
			)
		)
		
		;should be:
		;(list "vert0" "vert1" "vert2" "vert3")
		(concat
			"surprisals: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_nearest_generalized_distance
						4
						
						; k
						["x"]
						[0]
						1
						
						; p_parameter
						(null)
						
						; context_weights
						["continuous_number"]
						
						; types
						(null)
						
						; attributes
						[0.25]
						
						; context_deviations
						(null)
						"surprisal"
						
						; distance transform
						(null)
						
						; weight
						(rand)
						(null)
						"precise"
						.true
					)
				]
			)
		)
		
		;should be
		;(list "vert0" "vert2" "vert3" "vert1")
		(concat
			"weighted probabilities: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_nearest_generalized_distance
						4
						
						; k
						["x"]
						[0]
						1
						
						; p_parameter
						(null)
						
						; context_weights
						["continuous_number"]
						
						; types
						(null)
						
						; attributes
						[0.25]
						
						; context_deviations
						(null)
						"surprisal_to_prob"
						
						; distance transform
						"weight"
						
						; weight
						(rand)
						(null)
						"precise"
						.true
					)
				]
			)
		)
		
		;should be
		;(list "vert0" "vert2" "vert3" "vert1")
		(concat
			"weighted surprisals: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_nearest_generalized_distance
						4
						
						; k
						["x"]
						[0]
						1
						
						; p_parameter
						(null)
						
						; context_weights
						["continuous_number"]
						
						; types
						(null)
						
						; attributes
						[0.25]
						
						; context_deviations
						(null)
						"surprisal"
						
						; distance transform
						"weight"
						
						; weight
						(rand)
						(null)
						"precise"
						.true
					)
				]
			)
		)
		
		;(list "vert0" "vert1" "vert2")
		(concat
			"probabilities with dynamic selection: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_nearest_generalized_distance
						[0.01 1 20]
						
						; k
						["x"]
						[0]
						1
						
						; p_parameter
						(null)
						
						; context_weights
						["continuous_number"]
						
						; types
						(null)
						
						; attributes
						[0.25]
						
						; context_deviations
						(null)
						"surprisal_to_prob"
						
						; distance transform
						(null)
						
						; weight
						(rand)
						(null)
						"precise"
						.true
					)
				]
			)
		)
		
		;(list "vert0" "vert1" "vert2")
		(concat
			"surprisals with dynamic selection: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_nearest_generalized_distance
						[0.01 1 20]
						
						; k
						["x"]
						[0]
						1
						
						; p_parameter
						(null)
						
						; context_weights
						["continuous_number"]
						
						; types
						(null)
						
						; attributes
						[0.25]
						
						; context_deviations
						(null)
						"surprisal"
						
						; distance transform
						(null)
						
						; weight
						(rand)
						(null)
						"precise"
						.true
					)
				]
			)
		)
		
		;(list "vert0" "vert1" "vert2" "vert3")
		(concat
			"surprisals with dynamic selection: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_nearest_generalized_distance
						[0.01 1 20 1]
						
						; k
						["x"]
						[0]
						1
						
						; p_parameter
						(null)
						
						; context_weights
						["continuous_number"]
						
						; types
						(null)
						
						; attributes
						[0.25]
						
						; context_deviations
						(null)
						"surprisal"
						
						; distance transform
						(null)
						
						; weight
						(rand)
						(null)
						"precise"
						.true
					)
				]
			)
		)
		(concat
			"surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_distance_contributions
						4
						["x"]
						[
							[0]
						]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						(null)
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
		(concat
			"surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_distance_contributions
						[0.05 1 20]
						["x"]
						[
							[0]
						]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						(null)
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
		(concat
			"weighted surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_distance_contributions
						4
						["x"]
						[
							[0]
						]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						"weight"
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
		(concat
			"weighted surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_distance_contributions
						[0.05 1 20]
						["x"]
						[
							[0]
						]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						"weight"
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
		(create_entities
			["SurprisalTransformContainer" "testvert"]
			(lambda
				{weight 1 x 0}
			)
		)
		(concat
			"surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_entity_distance_contributions
						4
						["x"]
						["testvert"]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						(null)
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
		(concat
			"surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_entity_distance_contributions
						[0.05 1 20]
						["x"]
						["testvert"]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						(null)
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
		(concat
			"weighted surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_entity_distance_contributions
						4
						["x"]
						["testvert"]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						"weight"
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
		(concat
			"weighted surprisal contribution: "
			(compute_on_contained_entities
				"SurprisalTransformContainer"
				[
					(query_entity_distance_contributions
						[0.05 1 20]
						["x"]
						["testvert"]
						1
						(null)
						(null)
						(null)
						[0.25]
						(null)
						"surprisal"
						"weight"
						"fixed_seed"
						(null)
						"precise"
					)
				]
			)
		)
	]
))&", R"([
	"probabilities: 4"
	"surprisals: 4"
	"weighted probabilities: 4"
	"weighted surprisals: 4"
	"probabilities with dynamic selection: 2"
	"surprisals with dynamic selection: 2"
	"surprisals with dynamic selection: 3"
	"surprisal contribution: 1"
	"surprisal contribution: 1"
	"weighted surprisal contribution: 1"
	"weighted surprisal contribution: 1"
	[
		["SurprisalTransformContainer" "testvert"]
	]
	"surprisal contribution: 1"
	"surprisal contribution: 1"
	"weighted surprisal contribution: 1"
	"weighted surprisal contribution: 1"
])", "", R"((apply "destroy_entities" (contained_entities)))"},
AmalgamExample{ R"&((concat
	"+ : "
	||(+
		1
		2
		3
		4
		5
		6
		7
		8
		9
	)
))&", R"("+ : 45")" },
AmalgamExample{ R"&((concat
	"- : "
	||(-
		45
		1
		2
		3
		4
		5
		6
		7
		8
		9
	)
))&", R"("- : 0")" },
AmalgamExample{ R"&((concat
	"* : "
	||(* 1 2 3 4)
))&", R"("* : 24")" },
AmalgamExample{ R"&((concat
	"/ : "
	||(/ 24 1 2 3 4)
))&", R"("/ : 1")" },
AmalgamExample{ R"&((concat
	"mod : "
	||(mod 7 3)
))&", R"("mod : 1")" },
AmalgamExample{ R"&((concat
	"max : "
	||(max
		1
		2
		3
		4
		5
		6
		7
		8
		9
		10
	)
))&", R"("max : 10")" },
AmalgamExample{ R"&((concat
	"min : "
	||(min
		1
		2
		3
		4
		5
		6
		7
		8
		9
		10
	)
))&", R"("min : 1")" },
AmalgamExample{ R"&((concat
	"and : "
	||(and .true .true .true .true)
))&", R"("and : .true")" },
AmalgamExample{ R"&((concat
	"or : "
	||(or .false .false .true .false)
))&", R"("or : .true")" },
AmalgamExample{ R"&((concat
	"xor : "
	||(xor .true .false)
))&", R"("xor : .true")" },
AmalgamExample{ R"&((concat
	"= : "
	||(= 1 1 1 2)
))&", R"("= : .false")" },
AmalgamExample{ R"&((concat
	"!= : "
	||(!= 1 1 1 2)
))&", R"("!= : .false")" },
AmalgamExample{ R"&((concat
	"< : "
	||(< 1 1 1 2)
))&", R"("< : .false")" },
AmalgamExample{ R"&((concat
	"<= : "
	||(<= 1 1 1 2)
))&", R"("<= : .true")" },
AmalgamExample{ R"&((concat
	"> : "
	||(> 1 1 1 2)
))&", R"("> : .false")" },
AmalgamExample{ R"&((concat
	">= : "
	||(>= 1 1 1 2)
))&", R"(">= : .false")" },
AmalgamExample{ R"&((concat
	"~ : "
	||(~ 1 1 1 2)
))&", R"("~ : .true")" },
AmalgamExample{ R"&((concat
	"list : "
	||[
		(+ 1 0)
		1
		1
		2
	]
))&", R"("list : [1 1 1 2]")" },
AmalgamExample{ R"&((concat
	"associate : "
	||(associate
		"a"
		1
		"b"
		1
		"c"
		1
		"d"
		2
	)
))&", R"("associate : {a 1 b 1 c 1 d 2}")" },
AmalgamExample{ R"&((concat
	"assoc : "
	||{
		a (+ 1 0)
		b 1
		c 1
		d 2
	}
))&", R"("assoc : {a 1 b 1 c 1 d 2}")" },
AmalgamExample{ R"&((concat
	"map list : "
	||(map
		(lambda
			(* (current_value) 2)
		)
		[1 2 3 4]
	)
))&", R"("map list : [2 4 6 8]")" },
AmalgamExample{ R"&((concat
	"map assoc : "
	||(map
		(lambda
			(* (current_value) 2)
		)
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			"d"
			4
		)
	)
))&", R"("map assoc : {a 2 b 4 c 6 d 8}")" },
AmalgamExample{ R"&((concat
	"filter list : "
	||(filter
		(lambda
			(> (current_value) 2)
		)
		[1 2 3 4]
	)
))&", R"("filter list : [3 4]")" },
AmalgamExample{ R"&((concat
	"filter assoc : "
	||(filter
		(lambda
			(< (current_index) 20)
		)
		(associate
			10
			1
			20
			2
			30
			3
			40
			4
		)
	)
))&", R"("filter assoc : {10 1}")" },
AmalgamExample{ R"&((concat
	"filter assoc 2 : "
	||(filter
		(lambda
			(<= (current_value) 2)
		)
		(associate
			10
			1
			20
			2
			30
			3
			40
			4
		)
	)
))&", R"("filter assoc 2 : {10 1 20 2}")" },
AmalgamExample{ R"&(;nested concurrency
||(+
	||(map
		(lambda
			(let
				{
					index (current_value 1)
				}
				||(map
					(lambda
						(+ (current_index))
					)
					(range 1 100)
				)
			)
		)
		(range 1 100)
	)
))&", R"(100)" },
AmalgamExample{ R"&(;writing outside of concurrency
(let
	{x []}
	||(map
		(lambda
			(let
				{
					y (current_value 1)
				}
				(accum "x" [] y)
			)
		)
		(range 1 1000)
	)
	
	;Expecting 1000
	(size x)
))&", R"(1000)" },
AmalgamExample{ R"&((seq
	(create_entities
		"ConcurrentWritesTest"
		(lambda
			{concurrent_writes (unordered_list)}
		)
	)
	(set_entity_permissions "ConcurrentWritesTest" .true)
	||(map
		(lambda
			(accum_to_entities
				"ConcurrentWritesTest"
				{
					concurrent_writes (unordered_list
							(current_value 2)
						)
				}
			)
		)
		(range 1 1000)
	)
	
	;make sure the lists match up and none were lost
	(declare
		{
			result (=
					(set_type
						(range 1 1000)
						"unordered_list"
					)
					(retrieve_from_entity "ConcurrentWritesTest" "concurrent_writes")
				)
		}
	)
	(destroy_entities "ConcurrentWritesTest")
	result
))&", R"(.true)", "", R"((apply "destroy_entities" (contained_entities)))" },
AmalgamExample{ R"&((seq
	(create_entities "eq_distance_test" (null))
	(create_entities
		["eq_distance_test"]
		{x 0 y 0}
	)
	(create_entities
		["eq_distance_test" "to_delete1"]
		{x 1 y 0}
	)
	(create_entities
		["eq_distance_test"]
		{x 2 y 0}
	)
	(create_entities
		["eq_distance_test"]
		{x 3 y 0}
	)
	(create_entities
		["eq_distance_test"]
		{x 0 y 1}
	)
	(create_entities
		["eq_distance_test"]
		{x 0 y 2}
	)
	(create_entities
		["eq_distance_test"]
		{x 1 y 1}
	)
	(declare
		{
			result1 (map
					(lambda
						(retrieve_entity_root
							[
								"eq_distance_test"
								(current_value 1)
							]
						)
					)
					(contained_entities
						"eq_distance_test"
						[
							(query_within_generalized_distance
								1
								["x" "y"]
								[0 0]
							)
						]
					)
				)
		}
	)
	(create_entities
		["eq_distance_test" "to_delete2"]
		{x 0 y 0.5}
	)
	(declare
		{
			result2 (map
					(lambda
						(retrieve_entity_root
							[
								"eq_distance_test"
								(current_value 1)
							]
						)
					)
					(contained_entities
						"eq_distance_test"
						[
							(query_within_generalized_distance
								1
								["x" "y"]
								[0 0]
							)
						]
					)
				)
		}
	)
	(destroy_entities
		["eq_distance_test" "to_delete2"]
	)
	(destroy_entities
		["eq_distance_test" "to_delete1"]
	)
	(declare
		{
			result3 (map
					(lambda
						(retrieve_entity_root
							[
								"eq_distance_test"
								(current_value 1)
							]
						)
					)
					(contained_entities
						"eq_distance_test"
						[
							(query_within_generalized_distance
								1
								["x" "y"]
								[0 0]
							)
						]
					)
				)
		}
	)
	(destroy_entities "eq_distance_test")
	[result1 result2 result3]
))&", R"([
	[
		{x 0 y 0}
		{x 1 y 0}
		{x 0 y 1}
	]
	[
		{x 0 y 0}
		{x 1 y 0}
		{x 0 y 1}
		{x 0 y 0.5}
	]
	[
		{x 0 y 0}
		{x 0 y 1}
	]
])"}
);

//runs a test suite against the language
//the return value of this function will be returned for the executable
int32_t RunAmalgamLanguageValidation()
{
	Entity *entity = new Entity();
	entity->SetPermissions(ExecutionPermissions::AllPermissions(), ExecutionPermissions::AllPermissions(), true);

	std::vector<std::pair<std::string, size_t>> failed_test_names_and_numbers;

	//TODO 25157: replace with the top for loop when all are implemented
	for(size_t opcode_index = 0; opcode_index < NUM_VALID_ENT_OPCODES; opcode_index++)
	{
		EvaluableNodeType cur_opcode = static_cast<EvaluableNodeType>(opcode_index);
		std::string cur_opcode_str = GetStringFromEvaluableNodeType(cur_opcode, true);
		std::cout << "Validating opcode " << cur_opcode_str << std::endl;

		size_t num_examples = _opcode_details[opcode_index].examples.size();
		for(size_t test_number = 0; test_number < num_examples; test_number++)
		{
			auto &example = _opcode_details[opcode_index].examples[test_number];
			std::cout << "Test " << (test_number + 1) << " of " << num_examples << ": ";

			if(example.ValidateExample(entity))
				std::cout << "Passed" << std::endl;
			else
				failed_test_names_and_numbers.emplace_back(cur_opcode_str, test_number);
		}
	}

	//TODO 25157: put this back in when done with opcodes
	/*
	for(size_t unit_test_num = 0; unit_test_num < _amalgam_unit_tests.size(); unit_test_num++)
	{
		auto &unit_test = _amalgam_unit_tests[unit_test_num];
		std::cout << "Validating unit test " << (unit_test_num + 1) << " of " << _amalgam_unit_tests.size() << ": ";

		if(unit_test.ValidateExample(entity))
			std::cout << "Passed" << std::endl;
		else
			failed_test_names_and_numbers.emplace_back("unit test", unit_test_num);
	}//*/

	delete entity;

	if(failed_test_names_and_numbers.size() == 0)
	{
		std::cout << "----------------" << std::endl;
		std::cout << "All Tests Passed" << std::endl;
		return 0;
	}

	std::cout << "---------------------" << std::endl;
	std::cout << "Not All Tests Passed:" << std::endl;

	for(auto &[test_name, test_number] : failed_test_names_and_numbers)
		std::cerr << "Failed " << test_name << " test number " << (test_number + 1) << std::endl;

	return -1;
}
