#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"
#include "mmu.h"

char *strcpy(char *s, char *t) {
  char *os;

  os = s;
  while ((*s++ = *t++) != 0)
    ;
  return os;
}

int strcmp(const char *p, const char *q) {
  while (*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint strlen(char *s) {
  int n;

  for (n = 0; s[n]; n++)
    ;
  return n;
}

void *memset(void *dst, int c, uint n) {
  stosb(dst, c, n);
  return dst;
}

char *strchr(const char *s, char c) {
  for (; *s; s++)
    if (*s == c)
      return (char *)s;
  return 0;
}

char *gets(char *buf, int max) {
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    cc = read(0, &c, 1);
    if (cc < 1)
      break;
    buf[i++] = c;
    if (c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int stat(char *n, struct stat *st) {
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if (fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int atoi(const char *s) {
  int n;

  n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';
  return n;
}

void *memmove(void *vdst, void *vsrc, int n) {
  char *dst, *src;

  dst = vdst;
  src = vsrc;
  while (n-- > 0)
    *dst++ = *src++;
  return vdst;
}

// Atomic fetch and add
// Input: ptr = addr of the variable, and increment = value add to the variable
int fetch_and_add(int *ptr, int increment) {
  return __atomic_fetch_add(ptr, increment, __ATOMIC_SEQ_CST);
}

// adding user functions 

int thread_create(void (*start_routine)(void *, void *), void *arg1,
                  void *arg2) {

  // **** create new user stack using malloc, use PGSIZE variable for the size of the stack ****
  
  // **** use clone to create the child thread ****

  // **** pass it the newly created user stack ****

  // **** don't forget about the error checking ****
  
  // int result = clone(....);

  // if (result == -1)
  //   free(stack);

  // return result;
}

// int thread_join() {
//   // **** use join system call ****
//   // **** free the user stack ****
}


void lock_init(lock_t *lock) {
  // initialize lock_t tiket and turn to 0 
}

void lock_acquire(lock_t *lock) {
  // use fetch_and_add initilaize myturn  

 //  keep spinning until lock is acquired
}

void lock_release(lock_t *lock) {
  // increment the turn to release the lock
}