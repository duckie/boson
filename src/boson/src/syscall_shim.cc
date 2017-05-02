#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <stdio.h>

namespace boson {

namespace internal {
  class thread;
  thread*& current_thread();
}
}

#define EXPAND_AS_TYPE_1(T1,V1) T1
#define EXPAND_AS_ARGS_1(T1,V1) V1
#define EXPAND_AS_FUNC_1(T1,V1) T1 V1

#define EXPAND_AS_TYPE_PIPE1(T1,V1) T1[2]
#define EXPAND_AS_ARGS_PIPE1(T1,V1) V1
#define EXPAND_AS_FUNC_PIPE1(T1,V1) T1 V1[2]

#define EXPAND_AS_TYPE_2(T1,V1,T2,V2) T1,T2
#define EXPAND_AS_ARGS_2(T1,V1,T2,V2) V1,V2
#define EXPAND_AS_FUNC_2(T1,V1,T2,V2) T1 V1,T2 V2

#define EXPAND_AS_TYPE_PIPE2(T1,V1,T2,V2) T1[2],T2
#define EXPAND_AS_ARGS_PIPE2(T1,V1,T2,V2) V1,V2
#define EXPAND_AS_FUNC_PIPE2(T1,V1,T2,V2) T1 V1[2],T2 V2

#define EXPAND_AS_TYPE_3(T1,V1,T2,V2,T3,V3) T1,T2,T3
#define EXPAND_AS_ARGS_3(T1,V1,T2,V2,T3,V3) V1,V2,V3
#define EXPAND_AS_FUNC_3(T1,V1,T2,V2,T3,V3) T1 V1,T2 V2, T3 V3

#define EXPAND_AS_TYPE_4(T1,V1,T2,V2,T3,V3,T4,V4) T1,T2,T3,T4
#define EXPAND_AS_ARGS_4(T1,V1,T2,V2,T3,V3,T4,V4) V1,V2,V3,V4
#define EXPAND_AS_FUNC_4(T1,V1,T2,V2,T3,V3,T4,V4) T1 V1,T2 V2, T3 V3, T4 V4

#define EXPAND_AS_TYPE_5(T1,V1,T2,V2,T3,V3,T4,V4,T5,V5) T1,T2,T3,T4,T5
#define EXPAND_AS_ARGS_5(T1,V1,T2,V2,T3,V3,T4,V4,T5,V5) V1,V2,V3,V4,V5
#define EXPAND_AS_FUNC_5(T1,V1,T2,V2,T3,V3,T4,V4,T5,V5) T1 V1,T2 V2, T3 V3, T4 V4, T5 V5

#define DECLARE_SYSTEM_SYMBOL_SHIM(SYMBOL, RETURN_TYPE, NB_ARGS, ...) \
namespace boson { \
RETURN_TYPE SYMBOL(EXPAND_AS_TYPE_##NB_ARGS( __VA_ARGS__)); \
} \
static RETURN_TYPE (*sys_##SYMBOL)(EXPAND_AS_TYPE_##NB_ARGS( __VA_ARGS__)) = nullptr; \
extern "C" { \
RETURN_TYPE SYMBOL(EXPAND_AS_FUNC_##NB_ARGS( __VA_ARGS__)) { \
  if (boson::internal::current_thread()) { \
    return boson::SYMBOL(EXPAND_AS_ARGS_##NB_ARGS( __VA_ARGS__)); \
  } \
  if (!sys_##SYMBOL) { \
    sys_##SYMBOL = reinterpret_cast<decltype(sys_##SYMBOL)>(dlsym(RTLD_NEXT, #SYMBOL)); \
  } \
  return sys_##SYMBOL(EXPAND_AS_ARGS_##NB_ARGS( __VA_ARGS__)); \
}}

DECLARE_SYSTEM_SYMBOL_SHIM(sleep,unsigned int,1,unsigned int,duration);
DECLARE_SYSTEM_SYMBOL_SHIM(usleep,int,1,useconds_t,duration);
DECLARE_SYSTEM_SYMBOL_SHIM(nanosleep,int,2,const timespec*,req,timespec*,rem); 
DECLARE_SYSTEM_SYMBOL_SHIM(open,int,3,const char *,pathname,int,flags,mode_t,mode);
DECLARE_SYSTEM_SYMBOL_SHIM(creat,int,2,const char *,pathname,mode_t,mode);
DECLARE_SYSTEM_SYMBOL_SHIM(pipe,int,PIPE1,int,fds);
DECLARE_SYSTEM_SYMBOL_SHIM(pipe2,int,PIPE2,int,fds,int,flags);
DECLARE_SYSTEM_SYMBOL_SHIM(socket,int,3,int,domain,int,type,int,protocol);
DECLARE_SYSTEM_SYMBOL_SHIM(read,ssize_t,3,int,fd,void*,buffer,size_t,count);
DECLARE_SYSTEM_SYMBOL_SHIM(write,ssize_t,3,int,fd,const void*,buffer,size_t,count);
DECLARE_SYSTEM_SYMBOL_SHIM(accept,int,3,int,fd,sockaddr*,address,socklen_t*,count);
DECLARE_SYSTEM_SYMBOL_SHIM(send,ssize_t,4,int,fd,const void*,buffer,size_t,count,int,flags);
DECLARE_SYSTEM_SYMBOL_SHIM(recv,ssize_t,4,int,fd,void*,buffer,size_t,count,int,flags);
