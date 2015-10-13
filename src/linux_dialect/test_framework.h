extern cnt nthreads;

void launch_test(void *test(void *), const char *name);
void thr_sync(void);

err pause_universe(void);
void resume_universe(void);
void wait_for_universe();
