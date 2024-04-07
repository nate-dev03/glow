#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "object.h"
#include "exc.h"
#include "util.h"
#include "actor.h"

static struct glow_mailbox_node *make_node(GlowValue *v)
{
	struct glow_mailbox_node *node = glow_malloc(sizeof(struct glow_mailbox_node));
	glow_retain(v);
	node->value = *v;
	node->next = NULL;
	return node;
}

void glow_mailbox_init(struct glow_mailbox *mb)
{
	static GlowValue empty = GLOW_MAKE_EMPTY();
	struct glow_mailbox_node *node = make_node(&empty);
	mb->head = node;
	mb->tail = node;

	GLOW_SAFE(pthread_mutex_init(&mb->mutex, NULL));
	GLOW_SAFE(pthread_cond_init(&mb->cond, NULL));
}

void glow_mailbox_push(struct glow_mailbox *mb, GlowValue *v)
{
	struct glow_mailbox_node *node = make_node(v);
	GLOW_SAFE(pthread_mutex_lock(&mb->mutex));
	GLOW_SAFE(pthread_cond_signal(&mb->cond));
	struct glow_mailbox_node *prev = mb->head;
	mb->head = node;
	prev->next = node;
	GLOW_SAFE(pthread_mutex_unlock(&mb->mutex));
}

GlowValue glow_mailbox_pop(struct glow_mailbox *mb)
{
	struct glow_mailbox_node *tail = mb->tail;
	struct glow_mailbox_node *next = tail->next;

	if (next == NULL) {
		GLOW_SAFE(pthread_mutex_lock(&mb->mutex));
		while (mb->tail->next == NULL) {
			GLOW_SAFE(pthread_cond_wait(&mb->cond, &mb->mutex));
		}
		GLOW_SAFE(pthread_mutex_unlock(&mb->mutex));

		tail = mb->tail;
		next = tail->next;
	}

	mb->tail = next;
	free(tail);
	return next->value;
}

GlowValue glow_mailbox_pop_nowait(struct glow_mailbox *mb)
{
	struct glow_mailbox_node *tail = mb->tail;
	struct glow_mailbox_node *next = tail->next;

	if (next != NULL) {
		mb->tail = next;
		free(tail);
		return next->value;
	}

	return glow_makeempty();
}

void glow_mailbox_dealloc(struct glow_mailbox *mb)
{
	while (true) {
		GlowValue v = glow_mailbox_pop_nowait(mb);
		if (glow_isempty(&v)) {
			break;
		}
		glow_release(&v);
	}

	free(mb->head);
	mb->head = NULL;
	mb->tail = NULL;

	GLOW_SAFE(pthread_mutex_destroy(&mb->mutex));
	GLOW_SAFE(pthread_cond_destroy(&mb->cond));
}

GlowValue glow_actor_proxy_make(GlowCodeObject *co)
{
	GlowActorProxy *ap = glow_obj_alloc(&glow_actor_proxy_class);
	glow_retaino(co);
	ap->co = co;
	ap->defaults = (struct glow_value_array){.array = NULL, .length = 0};
	return glow_makeobj(ap);
}

static GlowActorObject *volatile actors = NULL;
static pthread_mutex_t link_mutex = PTHREAD_MUTEX_INITIALIZER;

static void actor_link(GlowActorObject *ao)
{
	GLOW_SAFE(pthread_mutex_lock(&link_mutex));
	glow_retaino(ao);

	if (actors != NULL) {
		actors->prev = ao;
	}

	ao->next = actors;
	ao->prev = NULL;
	actors = ao;
	GLOW_SAFE(pthread_mutex_unlock(&link_mutex));
}

static void actor_unlink(GlowActorObject *ao)
{
	GLOW_SAFE(pthread_mutex_lock(&link_mutex));
	glow_releaseo(ao);

	if (actors == ao) {
		actors = ao->next;
	}

	if (ao->prev != NULL) {
		ao->prev->next = ao->next;
	}

	if (ao->next != NULL) {
		ao->next->prev = ao->prev;
	}
	GLOW_SAFE(pthread_mutex_unlock(&link_mutex));
}

void glow_actor_join_all(void)
{
	while (true) {
		GLOW_SAFE(pthread_mutex_lock(&link_mutex));
		GlowActorObject *ao = actors;
		GLOW_SAFE(pthread_mutex_unlock(&link_mutex));

		if (ao != NULL) {
			GLOW_SAFE(pthread_join(ao->thread, NULL));
			actor_unlink(ao);
		} else {
			break;
		}
	}
}

GlowValue glow_actor_make(GlowActorProxy *gp)
{
	GlowActorObject *ao = glow_obj_alloc(&glow_actor_class);
	GlowCodeObject *co = gp->co;

	glow_mailbox_init(&ao->mailbox);

	GlowFrame *frame = glow_frame_make(co);
	frame->persistent = 1;
	frame->force_free_locals = 1;
	frame->mailbox = &ao->mailbox;

	glow_retaino(co);
	ao->co = gp->co;
	ao->frame = frame;
	ao->vm = glow_vm_new();
	ao->retval = glow_makeempty();
	ao->state = GLOW_ACTOR_STATE_READY;
	ao->next = NULL;
	ao->prev = NULL;
	return glow_makeobj(ao);
}

static void release_defaults(GlowActorProxy *ap);

static void actor_proxy_free(GlowValue *this)
{
	GlowActorProxy *ap = glow_objvalue(this);
	glow_releaseo(ap->co);
	release_defaults(ap);
	glow_obj_class.del(this);
}

static void actor_free(GlowValue *this)
{
	GlowActorObject *ao = glow_objvalue(this);

	if (ao->state == GLOW_ACTOR_STATE_RUNNING) {
		GLOW_SAFE(pthread_join(ao->thread, NULL));
		actor_unlink(ao);
	}

	glow_mailbox_dealloc(&ao->mailbox);
	glow_releaseo(ao->co);
	glow_frame_free(ao->frame);
	glow_vm_free(ao->vm);

	if (ao->retval.type == GLOW_VAL_TYPE_ERROR) {
		glow_err_free(glow_errvalue(&ao->retval));
	} else {
		glow_release(&ao->retval);
	}

	glow_obj_class.del(this);
}

void glow_actor_proxy_init_defaults(GlowActorProxy *ap, GlowValue *defaults, const size_t n_defaults)
{
	release_defaults(ap);
	ap->defaults.array = glow_malloc(n_defaults * sizeof(GlowValue));
	ap->defaults.length = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		ap->defaults.array[i] = defaults[i];
		glow_retain(&defaults[i]);
	}
}

static void release_defaults(GlowActorProxy *ap)
{
	GlowValue *defaults = ap->defaults.array;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = ap->defaults.length;
	for (size_t i = 0; i < n_defaults; i++) {
		glow_release(&defaults[i]);
	}

	free(defaults);
	ap->defaults = (struct glow_value_array){.array = NULL, .length = 0};
}

static GlowValue actor_proxy_call(GlowValue *this,
                                 GlowValue *args,
                                 GlowValue *args_named,
                                 size_t nargs,
                                 size_t nargs_named)
{
	GlowActorProxy *ap = glow_objvalue(this);
	GlowValue actor_v = glow_actor_make(ap);
	GlowActorObject *go = glow_objvalue(&actor_v);
	GlowCodeObject *co = ap->co;
	GlowFrame *frame = go->frame;
	GlowValue status = glow_codeobj_load_args(co,
	                                        &ap->defaults,
	                                        args,
	                                        args_named,
	                                        nargs,
	                                        nargs_named,
	                                        frame->locals);

	if (glow_iserror(&status)) {
		actor_free(&actor_v);
		return status;
	}

	return glow_makeobj(go);
}

static void *actor_start_routine(void *args)
{
	GlowActorObject *ao = args;
	GlowCodeObject *co = ao->co;
	GlowFrame *frame = ao->frame;
	GlowVM *vm = ao->vm;
	glow_current_vm_set(vm);

	glow_retaino(co);
	frame->co = co;

	glow_vm_push_frame_direct(vm, frame);
	glow_vm_eval_frame(vm);
	ao->retval = frame->return_value;
	glow_vm_pop_frame(vm);

	ao->frame = NULL;
	glow_frame_free(frame);

	ao->state = GLOW_ACTOR_STATE_FINISHED;
	return NULL;
}

GlowValue glow_actor_start(GlowActorObject *ao)
{
	if (ao->state != GLOW_ACTOR_STATE_READY) {
		return GLOW_ACTOR_EXC("cannot restart stopped actor");
	}

	actor_link(ao);
	ao->state = GLOW_ACTOR_STATE_RUNNING;
	if (pthread_create(&ao->thread, NULL, actor_start_routine, ao)) {
		actor_unlink(ao);
		ao->state = GLOW_ACTOR_STATE_FINISHED;
		return GLOW_ACTOR_EXC("could not spawn thread for actor");
	}

	return glow_makenull();
}

#define STATE_CHECK_NOT_FINISHED(ao) \
	if ((ao)->state == GLOW_ACTOR_STATE_FINISHED) \
		return GLOW_ACTOR_EXC("actor has been stopped")

#define RETURN_RETVAL(ao) \
	do { \
		GlowValue _temp = ao->retval; \
		if (ao->retval.type == GLOW_VAL_TYPE_ERROR) \
			ao->retval = glow_makeempty(); \
		else \
			glow_retain(&_temp); \
		return _temp; \
	} while (0)

static GlowValue actor_start(GlowValue *this,
                            GlowValue *args,
                            GlowValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "start"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowActorObject *ao = glow_objvalue(this);
	return glow_actor_start(ao);

#undef NAME
}

static GlowValue actor_check(GlowValue *this,
                            GlowValue *args,
                            GlowValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "check"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowActorObject *ao = glow_objvalue(this);

	if (ao->state != GLOW_ACTOR_STATE_FINISHED) {
		return glow_makenull();
	} else {
		RETURN_RETVAL(ao);
	}

#undef NAME
}

static GlowValue actor_join(GlowValue *this,
                           GlowValue *args,
                           GlowValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "join"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowActorObject *ao = glow_objvalue(this);

	if (ao->state != GLOW_ACTOR_STATE_READY) {
		GLOW_SAFE(pthread_join(ao->thread, NULL));
		actor_unlink(ao);
		RETURN_RETVAL(ao);
	} else {
		return GLOW_ACTOR_EXC("cannot join non-running actor");
	}

#undef NAME
}

static GlowValue actor_send(GlowValue *this,
                           GlowValue *args,
                           GlowValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "send"

	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);

	GlowActorObject *ao = glow_objvalue(this);
	STATE_CHECK_NOT_FINISHED(ao);
	GlowValue msg_v = glow_message_make(&args[0]);
	GlowMessage *msg = glow_objvalue(&msg_v);
	glow_mailbox_push(&ao->mailbox, &msg_v);
	GlowFutureObject *future = msg->future;
	glow_retaino(future);
	glow_releaseo(msg);
	return glow_makeobj(future);

#undef NAME
}

static GlowValue actor_stop(GlowValue *this,
                           GlowValue *args,
                           GlowValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "stop"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowActorObject *ao = glow_objvalue(this);
	STATE_CHECK_NOT_FINISHED(ao);

	GlowValue msg_v = glow_kill_message_make();
	GlowMessage *msg = glow_objvalue(&msg_v);
	glow_mailbox_push(&ao->mailbox, &msg_v);
	glow_releaseo(msg);
	return glow_makenull();

#undef NAME
}

struct glow_attr_method actor_methods[] = {
	{"start", actor_start},
	{"check", actor_check},
	{"join", actor_join},
	{"send", actor_send},
	{"stop", actor_stop},
	{NULL, NULL}
};


/* messages and futures */

GlowValue glow_future_make(void)
{
	GlowFutureObject *future = glow_obj_alloc(&glow_future_class);
	future->value = glow_makeempty();
	GLOW_SAFE(pthread_mutex_init(&future->mutex, NULL));
	GLOW_SAFE(pthread_cond_init(&future->cond, NULL));
	return glow_makeobj(future);
}

GlowValue glow_message_make(GlowValue *contents)
{
	GlowMessage *msg = glow_obj_alloc(&glow_message_class);
	glow_retain(contents);
	msg->contents = *contents;
	GlowValue future = glow_future_make();
	msg->future = glow_objvalue(&future);
	return glow_makeobj(msg);
}

GlowValue glow_kill_message_make(void)
{
	GlowValue empty = glow_makeempty();
	return glow_message_make(&empty);
}

static void future_set_value(GlowFutureObject *future, GlowValue *v)
{
	glow_release(&future->value);
	glow_retain(v);
	future->value = *v;
}

static void future_free(GlowValue *this)
{
	GlowFutureObject *future = glow_objvalue(this);
	glow_release(&future->value);
	GLOW_SAFE(pthread_mutex_destroy(&future->mutex));
	GLOW_SAFE(pthread_cond_destroy(&future->cond));
	glow_obj_class.del(this);
}

static void message_free(GlowValue *this)
{
	GlowMessage *msg = glow_objvalue(this);
	glow_release(&msg->contents);
	if (msg->future != NULL) {
		glow_releaseo(msg->future);
	}
	glow_obj_class.del(this);
}

static GlowValue future_get(GlowValue *this,
                           GlowValue *args,
                           GlowValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "get"

	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK_AT_MOST(NAME, nargs, 1);

	GlowFutureObject *future = glow_objvalue(this);
	bool timeout = false;

	if (nargs > 0) {
		if (!glow_isint(&args[0])) {
			GlowClass *class = glow_getclass(&args[0]);
			return GLOW_TYPE_EXC(NAME "() takes an integer argument (got a %s)", class->name);
		}

		const long ms = glow_intvalue(&args[0]);

		if (ms < 0) {
			GlowClass *class = glow_getclass(&args[0]);
			return GLOW_TYPE_EXC(NAME "() got a negative argument", class->name);
		}

		struct timespec ts;
		struct timeval tv;

		GLOW_SAFE(gettimeofday(&tv, NULL));

#ifndef TIMEVAL_TO_TIMESPEC
#define	TIMEVAL_TO_TIMESPEC(tv, ts)            \
	do {                                       \
		(ts)->tv_sec = (tv)->tv_sec;           \
		(ts)->tv_nsec = (tv)->tv_usec * 1000;  \
	} while (0)
#endif

		TIMEVAL_TO_TIMESPEC(&tv, &ts);
		ts.tv_sec += ms/1000;
		ts.tv_nsec += (ms % 1000) * 1000000;

		GLOW_SAFE(pthread_mutex_lock(&future->mutex));
		while (glow_isempty(&future->value)) {
			int n = pthread_cond_timedwait(&future->cond, &future->mutex, &ts);

			if (n == ETIMEDOUT) {
				timeout = true;
				break;
			} else if (n) {
				GLOW_INTERNAL_ERROR();
			}
		}
		GLOW_SAFE(pthread_mutex_unlock(&future->mutex));
	} else {
		GLOW_SAFE(pthread_mutex_lock(&future->mutex));
		while (glow_isempty(&future->value)) {
			GLOW_SAFE(pthread_cond_wait(&future->cond, &future->mutex));
		}
		GLOW_SAFE(pthread_mutex_unlock(&future->mutex));
	}

	if (timeout) {
		return GLOW_ACTOR_EXC(NAME "() timed out");
	} else {
		glow_retain(&future->value);
		return future->value;
	}

#undef NAME
}

static GlowValue message_contents(GlowValue *this,
                                 GlowValue *args,
                                 GlowValue *args_named,
                                 size_t nargs,
                                 size_t nargs_named)
{
#define NAME "contents"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowMessage *msg = glow_objvalue(this);
	glow_retain(&msg->contents);
	return msg->contents;

#undef NAME
}

static GlowValue message_reply(GlowValue *this,
                              GlowValue *args,
                              GlowValue *args_named,
                              size_t nargs,
                              size_t nargs_named)
{
#define NAME "reply"

	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);

	GlowMessage *msg = glow_objvalue(this);
	GlowFutureObject *future = msg->future;
	GlowValue ret;

	GLOW_SAFE(pthread_mutex_lock(&future->mutex));
	if (future != NULL) {
		future_set_value(future, &args[0]);
		GLOW_SAFE(pthread_cond_broadcast(&future->cond));
		glow_releaseo(future);
		msg->future = NULL;
		ret = glow_makenull();
	} else {
		ret = GLOW_ACTOR_EXC("cannot reply to the same message twice");
	}
	GLOW_SAFE(pthread_mutex_unlock(&future->mutex));
	return ret;

#undef NAME
}

struct glow_attr_method future_methods[] = {
	{"get", future_get},
	{NULL, NULL}
};

struct glow_attr_method message_methods[] = {
	{"contents", message_contents},
	{"reply", message_reply},
	{NULL, NULL}
};


/* classes */

GlowClass glow_actor_proxy_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "ActorProxy",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowActorProxy),

	.init = NULL,
	.del = actor_proxy_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = actor_proxy_call,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_actor_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Actor",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowActorObject),

	.init = NULL,
	.del = actor_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = actor_methods,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_future_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Future",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowFutureObject),

	.init = NULL,
	.del = future_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = future_methods,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_message_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Message",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowMessage),

	.init = NULL,
	.del = message_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = message_methods,

	.attr_get = NULL,
	.attr_set = NULL
};
