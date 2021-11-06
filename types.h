typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

// ################ lock_t struct ############################
typedef struct {
  int ticket;
  int turn;
} lock_t;
// ###########################################################