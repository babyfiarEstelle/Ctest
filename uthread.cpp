
#include <stdio.h>
#include <vector>
//#include <ucontext.h>
#include<signal.h>

#define DEFAULT_STACK_SZIE (1024*128)
#define MAX_UTHREAD_SIZE   1024

//#ifndef MY_UTHREAD_CPP
#define MY_UTHREAD_CPP
enum ThreadState { FREE, RUNNABLE, RUNNING, SUSPEND };

struct schedule_t;

typedef void(*Fun)(void *arg);

typedef struct uthread_t
{
	ucontext_t ctx;
	Fun func;
	void *arg;
	enum ThreadState state;
	char stack[DEFAULT_STACK_SZIE];
}uthread_t;

typedef struct schedule_t
{
	ucontext_t main;
	int running_thread;
	uthread_t *threads;
	int max_index; // 曾经使用到的最大的index + 1

	schedule_t() :running_thread(-1), max_index(0) {
		threads = new uthread_t[MAX_UTHREAD_SIZE];
		for (int i = 0; i < MAX_UTHREAD_SIZE; i++) {
			threads[i].state = FREE;
		}
	}

	~schedule_t() {
		delete[] threads;
	}
}schedule_t;

void uthread_resume(schedule_t &schedule, int id)
{
	if (id < 0 || id >= schedule.max_index) {
		return;
	}

	uthread_t *t = &(schedule.threads[id]);

	if (t->state == SUSPEND) {
		swapcontext(&(schedule.main), &(t->ctx));
	}
}

void uthread_yield(schedule_t &schedule)
{
	if (schedule.running_thread != -1) {
		uthread_t *t = &(schedule.threads[schedule.running_thread]);
		t->state = SUSPEND;
		schedule.running_thread = -1;

		swapcontext(&(t->ctx), &(schedule.main));
	}
}

void uthread_body(schedule_t *ps)
{
	int id = ps->running_thread;

	if (id != -1) {
		uthread_t *t = &(ps->threads[id]);

		t->func(t->arg);

		t->state = FREE;

		ps->running_thread = -1;
	}
}

int uthread_create(schedule_t &schedule, Fun func, void *arg)
{
	int id = 0;

	for (id = 0; id < schedule.max_index; ++id) {
		if (schedule.threads[id].state == FREE) {
			break;
		}
	}

	if (id == schedule.max_index) {
		schedule.max_index++;
	}

	uthread_t *t = &(schedule.threads[id]);

	t->state = RUNNABLE;
	t->func = func;
	t->arg = arg;

	getcontext(&(t->ctx));

	t->ctx.uc_stack.ss_sp = t->stack;
	t->ctx.uc_stack.ss_size = DEFAULT_STACK_SZIE;
	t->ctx.uc_stack.ss_flags = 0;
	t->ctx.uc_link = &(schedule.main);
	schedule.running_thread = id;

	makecontext(&(t->ctx), (void(*)(void))(uthread_body), 1, &schedule);
	swapcontext(&(schedule.main), &(t->ctx));

	return id;
}

int schedule_finished(const schedule_t &schedule)
{
	if (schedule.running_thread != -1) {
		return 0;
	}
	else {
		for (int i = 0; i < schedule.max_index; ++i) {
			if (schedule.threads[i].state != FREE) {
				return 0;
			}
		}
	}

	return 1;
}

void func1(void * arg)
{
	puts("!");
	puts("!!");
	puts("!!");
	puts("!!!!");

}

void func2(void * arg)
{
	puts("2");
	puts("2");
	uthread_yield(*(schedule_t *)arg);
	puts("22");
	puts("22");
}

void func3(void *arg)
{
	puts("33");
	puts("33");
	uthread_yield(*(schedule_t *)arg);
	puts("3333");
	puts("3333");

}

void context_test()
{
	char stack[1024 * 128];
	ucontext_t uc1, ucmain;

	getcontext(&uc1);
	uc1.uc_stack.ss_sp = stack;
	uc1.uc_stack.ss_size = 1024 * 128;
	uc1.uc_stack.ss_flags = 0;
	uc1.uc_link = &ucmain;

	makecontext(&uc1, (void(*)(void))func1, 0);

	swapcontext(&ucmain, &uc1);
	puts("main");
}

void schedule_test()
{
	schedule_t s;

	int id1 = uthread_create(s, func3, &s);
	int id2 = uthread_create(s, func2, &s);

	while (!schedule_finished(s)) {
		uthread_resume(s, id2);
		uthread_resume(s, id1);
	}
	puts("main over");

}
int main()
{

	context_test();
	puts("----------------");
	schedule_test();

	return 0;
}