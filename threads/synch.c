/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters); // 세마포어 대기열 초기화
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) { // sema_down(스레드가 자원 점유)
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable (); // 인터럽트 비활성화
	/*
	세마포어가 0이면 세마포어가 가용할 때까지 자원에 접근할 수 없음
	자원에 접근할 수 없기 때문에 waiters 리스트에 추가됨
	*/
	while (sema->value == 0) { // 세마포어가 0이면 waiters 리스트에 추가
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		list_insert_ordered(&sema->waiters, &thread_current()->elem, thread_compare_priority, 0); // waiters 리스트가 priority 내림차순으로 정렬되도록 insert
		thread_block (); // 현재 스레드 blocked 처리
	}
	sema->value--;
	intr_set_level (old_level); // 인터럽트 활성화
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/*
세마포어의 waiters 리스트의 thread끼리 priority를 비교하는 함수
*/
bool semaphore_compare_priority(const struct list_elem *l, const struct list_elem *s, void *aux UNUSED) {

	struct semaphore_elem *l_sema = list_entry(l, struct semaphore_elem, elem);
	struct semaphore_elem *s_sema = list_entry(s, struct semaphore_elem, elem);

	struct list *waiter_l_sema = &(l_sema->semaphore.waiters);
	struct list *waiter_s_sema = &(s_sema->semaphore.waiters);

	return list_entry(list_begin(waiter_l_sema), struct thread, elem)->priority 
	> list_entry(list_begin(waiter_s_sema), struct thread, elem)->priority;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) { // sema_up(스레드가 자원 점유 해제)
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable (); // 인터럽트 비활성화
	if (!list_empty (&sema->waiters)) // waiters 리스트가 비지 않았다면
	{
		/*
		리스트의 첫 번째 원소 빼기 전에 waiters 내의 스레드들의 priority 변화가
		생겼을 수도 있으므로 priority 내림차순으로 정렬을 한 번 해준다
		*/
		list_sort(&sema->waiters, thread_compare_priority, 0);

		// waiter 리스트의 가장 앞 스레드 unblock
		thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
	}
	sema->value++; // 세마포어 값 +1
	/*
	unblock 된 스레드의 priority가
	running 중인 스레드의 priority 보다 높을 수도 있으므로
	체크해서 컨텍스트 스위칭이 일어날 수 있도록 thread_preemption_check()
	*/
	thread_preemption_check();
	intr_set_level (old_level); // 인터럽트 활성화
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock)); // 락이 현재 스레드에 의해 점유되었는지 체크
	
	// thread_current()->priority = ; // 현재 running 중인 스레드의 priority를 ready_list의 스레드의 priority로부터 donate 받음
	// 현재 running 스레드와 ready_list의 락을 획득하려고 하는 스레드의 priority 비교

	struct thread *curr = thread_current(); // 락을 점유하려고는 스레드
	if(lock->holder) { // 락을 현재 점유하고 있는 스레드가 존재하면
		curr->waiting_lock = lock; // 락을 점유하려고 하는 스레드의 waiting_lock에 해당 락을 추가해준다.
		/*
		donation이 발생하기 때문에 락을 점유중인 스레드의 donations 리스트에 락을 점유하려고 하는 스레드를 추가를 해준다.
		이 때 donations 리스트에 priority가 높은 순으로 정렬될 수 있도록 list_insert_ordered 함수로 추가를 해준다.
		*/
		list_insert_ordered(&lock->holder->donations, &curr->donation_elem, thread_compare_donate_priority, 0);
		donate_priority(); // priority 도네이션
	}
	sema_down (&lock->semaphore); // 락을 획득하기 때문에 sema down

	curr->waiting_lock = NULL; // 해당 스레드가 락을 점유했기 때문에 waiting_lock을 NULL 초기화 해준다.
	lock->holder = curr; // 락 소유자는 현재 스레드로 설정
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */

/*
락을 점유 중이던 스레드가 작업을 끝내고 락을 반환할 경우
priority donation을 받은 상태라면 스레드의 donations 리스트에서
priority donation을 받았던 스레드를 제거
*/
void 
remove_priority_donation(struct lock *lock) {
	struct list_elem *e;
	struct thread *curr = thread_current(); // 현재 스레드
	
	// 스레드의 donations 리스트를 순회하면서
	for(e = list_begin(&curr->donations); e != list_end(&curr->donations); e = list_next(e)) {
		// 리스트 내의 스레드의 waiting_lock이 현재 스레드가 반환하는 lock과 같으면
		struct thread *t = list_entry(e, struct thread, donation_elem);
		if(t->waiting_lock == lock) {
			list_remove(&t->donation_elem); // donations 리스트에서 스레드를 제거해준다.
		}
	}
}

/*
락을 점유 중이던 스레드가 작업을 끝내고 락을 반환하고
priority donation을 받았었다면
기존 스레드가 가지던 초기 priority 값으로 재설정
*/
void
reset_priority(void) {
	struct thread *curr = thread_current(); // 현재 스레드
	curr->priority = curr->my_priority; // 현재 스레드의 priority를 초기 priority로 재설정(initial priority)

	// 현재 스레드의 donations 리스트를 priority 내림차순으로 재정렬
	if(!list_empty(&curr->donations)) {
		list_sort(&curr->donations, thread_compare_donate_priority, 0);
	}

	// donations 리스트에서 맨 앞의 스레드를 뽑음
	struct thread *front = list_entry(list_front(&curr->donations), struct thread, donation_elem);

	// 맨 앞의 스레드의 priority와 현재 스레드의 priority를 비교
	if(front->priority > curr->priority) {
		// 맨 앞의 priority가 더 크다면 현재 스레드의 priority로 설정
		curr->priority = front->priority;
	}
}

/*
priority를 donate 받아 작업을 수행하던 스레드가
lock을 반환
*/
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	remove_priority_donation(lock); // 스레드의 락 점유가 끝나 donation 받은 priority를 제거
	reset_priority(); // 스레드의 이전의 priority로 재설정

	lock->holder = NULL; // 락의 홀더를 NULL 처리
	sema_up (&lock->semaphore); // 락이 반환됐으므로 sema up
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
	int64_t semaphore_elem_priority; // waiter 리스트 내의 세마포어의 priority
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock)); // 현재 스레드가 락 점유하는지 확인

	/* 대기자(웨이터) 세마포어 초기화 */
	sema_init (&waiter.semaphore, 0); // 웨이터의 세마포어를 0으로 초기화
	waiter.semaphore_elem_priority = thread_current()->priority;

	// list_push_back (&cond->waiters, &waiter.elem); // 조건 변수의 waiter리스트에 초기화한 웨이터를 추가

	// 웨이트 리스트에 초기화한 웨이터를 추가 시 semaphore의 priority를 기준으로 정렬될 수 있도록 처리
	list_insert_ordered(&cond->waiters, &waiter.elem, semaphore_compare_priority, 0);
	lock_release (lock); // 현재 스레드가 점유하고 있던 락을 해제
	sema_down (&waiter.semaphore); // 현재 웨이터의 세마포어를 -1
	lock_acquire (lock); // 락의 holder는 락을 획득한 현재 스레드가 됨
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	/*
	wait 리스트 내의 기다리고 있는 스레드 하나에게 신호를 보내 깨운다.
	*/
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	// cond의 waiter 리스트가 비지 않았으면
	if (!list_empty (&cond->waiters)) {
		list_sort(&cond->waiters, semaphore_compare_priority, 0);
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}