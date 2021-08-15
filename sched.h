#include <uapi/linux/time.h>

struct time_statistics{
	struct timeval *in_cpu;
	struct timeval *out_cpu;
	struct timeval *in_context_switch;
	struct timeval *out_context_switch;
	struct timeval *in_queue_start;
	struct timeval *in_queue_end;
	struct timeval *out_queue_start;
	struct timeval *out_queue_end;
	struct timeval *p_n_t_start;
	struct timeval *p_n_t_end;
	struct timeval *next_p_n_t_start;
};

struct task_struct {
	
	struct time_statistics *time_s;
	
	/*
	 *
	 */
}
