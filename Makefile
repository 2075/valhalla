CC=clang++
FLAGS=-std=c++11
LIBS=-lvalhalla_midgard -lvalhalla_baldr -lvalhalla_loki -lvalhalla_sif


.PHONY: compile_tests


compile_tests: test_queue test_viterbi_search test_map_matching test_edge_search test_grid_range_query test_sp test_service


test_service: service.cc
	$(CC) $(FLAGS) $(LIBS) -lprime_server -lpthread $< -o $@


test_sp: test_sp.cc sp.h
	$(CC) $(FLAGS) -lvalhalla_midgard -lvalhalla_baldr $< -o $@


test_edge_search: test_edge_search.cc edge_search.h candidate.h grid_range_query.h
	$(CC) $(FLAGS) $(LIBS) $< -o $@


test_grid_range_query: test_grid_range_query.cc grid_range_query.h
	$(CC) $(FLAGS) -lvalhalla_midgard $< -o $@


test_map_matching: test_map_matching.cc map_matching.h edge_search.h viterbi_search.h queue.h candidate.h costings.h sp.h
	$(CC) $(FLAGS) $(LIBS) $< -o $@


test_queue: test_queue.cc queue.h
	$(CC) $(FLAGS) $< -o $@


test_viterbi_search: test_viterbi_search.cc viterbi_search.h queue.h
	$(CC) $(FLAGS) $< -o $@
