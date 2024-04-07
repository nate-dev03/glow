#ifndef GLOW_ACTOR_H
#define GLOW_ACTOR_H

#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include "object.h"
#include "codeobject.h"
#include "vm.h"
#include "err.h"

struct glow_mailbox_node {
	GlowValue value;
	struct glow_mailbox_node *volatile next;
};

struct glow_mailbox {
	struct glow_mailbox_node *volatile head;
	struct glow_mailbox_node *tail;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

void glow_mailbox_init(struct glow_mailbox *mb);
void glow_mailbox_push(struct glow_mailbox *mb, GlowValue *v);
GlowValue glow_mailbox_pop(struct glow_mailbox *mb);
GlowValue glow_mailbox_pop_nowait(struct glow_mailbox *mb);
void glow_mailbox_dealloc(struct glow_mailbox *mb);

extern GlowClass glow_actor_proxy_class;
extern GlowClass glow_actor_class;
extern GlowClass glow_future_class;
extern GlowClass glow_message_class;

typedef struct {
	GlowObject base;
	GlowCodeObject *co;
	struct glow_value_array defaults;
} GlowActorProxy;

typedef struct glow_actor_object {
	GlowObject base;
	struct glow_mailbox mailbox;

	GlowCodeObject *co;
	GlowFrame *frame;
	GlowVM *vm;
	GlowValue retval;

	pthread_t thread;

	enum {
		GLOW_ACTOR_STATE_READY,
		GLOW_ACTOR_STATE_RUNNING,
		GLOW_ACTOR_STATE_FINISHED
	} state;

	struct glow_actor_object *prev;
	struct glow_actor_object *next;
} GlowActorObject;

typedef struct {
	GlowObject base;

	GlowValue value;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} GlowFutureObject;

typedef struct {
	GlowObject base;
	GlowValue contents;  /* empty contents = kill message */
	GlowFutureObject *future;
} GlowMessage;

GlowValue glow_actor_proxy_make(GlowCodeObject *co);
GlowValue glow_actor_make(GlowActorProxy *ap);
void glow_actor_proxy_init_defaults(GlowActorProxy *ap, GlowValue *defaults, const size_t n_defaults);
void glow_actor_join_all(void);

GlowValue glow_future_make(void);
GlowValue glow_message_make(GlowValue *contents);
GlowValue glow_kill_message_make(void);

#endif /* GLOW_ACTOR_H */
