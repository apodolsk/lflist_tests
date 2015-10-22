#define LOG_TESTM 0

extern cnt nthreads;
static const uptr firstborn = 2;

void launch_test(void *test(void *), const char *name);
                                                       
struct syncx;
typedef struct syncx syncx;
void syncx_init(syncx *s, cnt expected);
void thr_sync(syncx* x);
syncx *start_timing;
syncx *stop_timing;

err pause_universe(void);
void resume_universe(void);
void wait_for_universe();

#define thr_sync(as...) trace(TESTM, 1, thr_sync, as)
#define pause_universe(as...) trace(TESTM, 1, pause_universe, as)
