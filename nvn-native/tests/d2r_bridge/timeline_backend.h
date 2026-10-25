#pragma once
void nn_test_fail_next(int);
void nn_test_faults(int);
unsigned nn_test_accepted(void);
unsigned nn_test_dependencies(void);
unsigned nn_test_flushes(void);
unsigned nn_test_waits(void);
unsigned nn_test_cache_calls(void);
void nn_test_retire(nn_queue *,uint32_t);

void nn_test_ambiguous_next(void);
void nn_test_resolve_unknown(nn_queue*);
unsigned nn_test_destroy_count(const void*);
int nn_test_last_has(nn_bo*);

nn_bo *nn_test_last_parent(void);
