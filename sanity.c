#include "types.h"
#include "stat.h"
#include "user.h"

int global_num = 3224; // Should be able to be accessed by threads

// Tests the basics, arg passing, arg returning
void swap(char *a_string, int *a_number) {
  printf(1, "Thread pid [%d] Arg1 [%s] Arg2 [%d] global_num [%d]\n", getpid(),
         a_string, *a_number, global_num);
  int temp = global_num; // Swap global_num and a_number
  global_num = *a_number;
  *a_number = temp;
  exit();
}

// Testing lock
void counter(lock_t *lock, int *loop) {
  if (lock == 0) { // No lock
    for (int i = 0; i < *loop; i++) {
      printf(1, "Without Lock pid [%d] i [%d]\n", getpid(), i);
    }
    exit();
  } else { // Lock
    lock_acquire(lock);
    for (int i = 0; i < *loop; i++) {
      printf(1, "With Lock pid [%d] i [%d]\n", getpid(), i);
    }
    lock_release(lock);
    exit();
  }
}

// Testing memory allocation
void memory_allocation(int *size, int *loop) {
  int *arr;
  for (int i = 0; i < *loop; i++) {
    // Allocate memory
    arr = malloc(*size * sizeof(int));
    // Write to memory
    for (int j = 0; j < *size; j++) {
      arr[j] = 1;
    }
    // Free memory
    free(arr);
  }
  exit();
}

int main() {
  printf(1, "#########################################################\n");
  printf(1, "Testing Thread Arg Passing, Returning, and Shared Memory\n");
  printf(1, "#########################################################\n");

  char *a_string = "Sanity test sanity test sanity test";
  int a_number = 1234;
  printf(1, "Main thread pid [%d]\n", getpid());
  printf(1, "Before swap() global_num [%d] a_number [%d]\n", global_num,
         a_number);
  thread_create((void (*)(void *, void *))swap, a_string, &a_number);
  int thread_pid = thread_join();
  printf(1, "After swap() global_num [%d] a_number [%d]\n", global_num,
         a_number);
  printf(1, "Returned thread pid [%d]\n", thread_pid);

  printf(1, "\n#########################################################\n");
  printf(1, "Testing Lock\n");
  printf(1, "#########################################################\n");

  lock_t lock;
  lock_init(&lock);

  int loop = 5;
  // Without lock - print msg between 2 threads should interleave
  thread_create((void (*)(void *, void *))counter, 0, &loop);
  thread_create((void (*)(void *, void *))counter, 0, &loop);
  thread_create((void (*)(void *, void *))counter, 0, &loop);
  thread_join();
  thread_join();
  thread_join();
  // With lock - print msgs between 2 threads should NOT interleave
  printf(1, "\n");
  thread_create((void (*)(void *, void *))counter, &lock, &loop);
  thread_create((void (*)(void *, void *))counter, &lock, &loop);
  thread_create((void (*)(void *, void *))counter, &lock, &loop);
  thread_join();
  thread_join();
  thread_join();

  printf(1, "\n#########################################################\n");
  printf(1, "Testing Memory Allocation\n");
  printf(1, "#########################################################\n");
  int size = 2048;
  loop = 50;
  thread_create((void (*)(void *, void *))memory_allocation, &size, &loop);
  loop = 100;
  thread_create((void (*)(void *, void *))memory_allocation, &size, &loop);
  thread_join();
  thread_join();
  printf(1, "Testing Memory Allocation Completed\n");
  exit();
}