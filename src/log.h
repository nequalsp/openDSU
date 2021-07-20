#ifndef DSU_LOG
#define DSU_LOG


#ifdef DEBUG
#define DSU_DEBUG_PRINT(format, ...) { fprintf(dsu_program_state.logfd, format, ## __VA_ARGS__); fflush(dsu_program_state.logfd);}
#else
#define DSU_DEBUG_PRINT(format, ...)
#endif


#endif
