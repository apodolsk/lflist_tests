extern cnt nthreads;

void launch_test(void *test(void *));
void thr_setup(uint id);
void thr_sync(void);
void thr_destroy(uint id);

err pause_universe(void);
void resume_universe(void);
void wait_for_universe();
