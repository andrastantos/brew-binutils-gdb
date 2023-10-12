extern "C" {
#include "defs.h"
}

#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#include <time.h>
#if ! defined __MINGW32__
#include <sys/times.h>
#endif // __MINGW32__
#include <errno.h>
#include <math.h>

extern "C" {
#include "defs.h"

#define basename basename
#include "bfd.h"
#undef basename

#include "libiberty.h"
#include "sim/sim.h"

#include "sim-main.h"
#include "sim-base.h"
#include "sim-options.h"
#include "sim-io.h"
#include "sim-signal.h"
#include "models.h"
#include "sim-hw.h"
#include "opcode/brew-abi.h"
#include "newlib_syscalls.h"
#include "newlib_stat.h"
#include "brew-calls.h"
}

typedef enum {
  acc_read,
  acc_write,
  acc_exec,
} brew_access_modes;

typedef void (*v2p_addr_ftype)(sim_cpu *scpu, uint32_t v_addr, brew_access_modes access_mode, uint32_t *p_addr);

static const uint16_t BREW_CSR_MARCH                   = 0x8000;
static const uint16_t BREW_CSR_CAPABILITY              = 0x8001;
static const uint16_t BREW_CSR_FPSTAT                  = 0x8010;

static const uint16_t BREW_CSR_ECAUSE                  = 0x0000;
static const uint16_t BREW_CSR_EADDR                   = 0x0001;

static const uint16_t ESPRESSO_CSR_PMEM_BASE_REG       = 0x0080;
static const uint16_t ESPRESSO_CSR_PMEM_LIMIT_REG      = 0x0081;
static const uint16_t ESPRESSO_CSR_DMEM_BASE_REG       = 0x0082;
static const uint16_t ESPRESSO_CSR_DMEM_LIMIT_REG      = 0x0083;

static const uint16_t ESPRESSO_CSR_BUS_IF_CFG_REG      = 0x0200;

static const uint16_t ESPRESSO_CSR_DMA_CHA_0_ADDR      = 0x0300;
static const uint16_t ESPRESSO_CSR_DMA_CHA_0_LIMIT     = 0x0301;
static const uint16_t ESPRESSO_CSR_DMA_CHA_0_CONFIG    = 0x0302;
static const uint16_t ESPRESSO_CSR_DMA_CHA_0_STATUS    = 0x0303;
static const uint16_t ESPRESSO_CSR_DMA_CHA_1_ADDR      = 0x0304;
static const uint16_t ESPRESSO_CSR_DMA_CHA_1_LIMIT     = 0x0305;
static const uint16_t ESPRESSO_CSR_DMA_CHA_1_CONFIG    = 0x0306;
static const uint16_t ESPRESSO_CSR_DMA_CHA_1_STATUS    = 0x0307;
static const uint16_t ESPRESSO_CSR_DMA_CHA_2_ADDR      = 0x0308;
static const uint16_t ESPRESSO_CSR_DMA_CHA_2_LIMIT     = 0x0309;
static const uint16_t ESPRESSO_CSR_DMA_CHA_2_CONFIG    = 0x030a;
static const uint16_t ESPRESSO_CSR_DMA_CHA_2_STATUS    = 0x030b;
static const uint16_t ESPRESSO_CSR_DMA_CHA_3_ADDR      = 0x030c;
static const uint16_t ESPRESSO_CSR_DMA_CHA_3_LIMIT     = 0x030d;
static const uint16_t ESPRESSO_CSR_DMA_CHA_3_CONFIG    = 0x030e;
static const uint16_t ESPRESSO_CSR_DMA_CHA_3_STATUS    = 0x030f;
static const uint16_t ESPRESSO_CSR_DMA_INT_STAT        = 0x0310;

static const uint16_t ESPRESSO_CSR_TIMER_VAL_LIMIT     = 0x0400;
static const uint16_t ESPRESSO_CSR_TIMER_INT_STATUS    = 0x0401;
static const uint16_t ESPRESSO_CSR_TIMER_CTRL          = 0x0402;

static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_0  = 0x8100;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_0     = 0x8102;
static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_1  = 0x8103;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_1     = 0x8104;
static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_2  = 0x8105;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_2     = 0x8106;
static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_3  = 0x8107;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_3     = 0x8108;
static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_4  = 0x8109;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_4     = 0x810a;
static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_5  = 0x810b;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_5     = 0x810c;
static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_6  = 0x810d;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_6     = 0x810e;
static const uint16_t ESPRESSO_CSR_EVENT_SELECT_REG_7  = 0x810f;
static const uint16_t ESPRESSO_CSR_EVENT_CNT_REG_7     = 0x8110;


static const uint32_t ANACHRON_IO_GPIO_BASE     = 0x00010000;
static const uint32_t ANACHRON_IO_APB_BASE      = 0x00020000;

static const uint32_t ANACHRON_UART_BASE        = ANACHRON_IO_APB_BASE + 0x0000;
static const uint32_t ANACHRON_UART_SIZE        = 5;

static const uint32_t ANACHRON_GPIO_3_BASE      = ANACHRON_IO_APB_BASE + 0x0100;
static const uint32_t ANACHRON_GPIO_3_SIZE      = 1;

static const uint32_t ANACHRON_GPIO_4_BASE      = ANACHRON_IO_APB_BASE + 0x0200;
static const uint32_t ANACHRON_GPIO_4_SIZE      = 1;

static const uint32_t ANACHRON_GPIO_INT_BASE    = ANACHRON_IO_GPIO_BASE + 0x2000;
static const uint32_t ANACHRON_GPIO_INT_SIZE    = 1;

enum {
  OPTION_ESPRESSO_ROM_FILE = OPTION_START,
};



static int marshal_o_flags_from_sim(int oflags)
{
  #define  NEWLIB_O_RDONLY    0x000000
  #define  NEWLIB_O_WRONLY    0x000001
  #define  NEWLIB_O_RDWR      0x000002
  #define  NEWLIB_O_APPEND    0x000008
  #define  NEWLIB_O_CREAT     0x000200
  #define  NEWLIB_O_TRUNC     0x000400
  #define  NEWLIB_O_EXCL      0x000800
  #define  NEWLIB_O_SYNC      0x002000
  #define  NEWLIB_O_NONBLOCK  0x004000
  #define  NEWLIB_O_NOCTTY    0x008000
  #define  NEWLIB_O_CLOEXEC   0x040000
  #define  NEWLIB_O_NOFOLLOW  0x100000
  #define  NEWLIB_O_DIRECTORY 0x200000
  //#define  NEWLIB_O_EXEC      0x400000 - no direct equivalent on host?
  int ret_val = 0;
  if ((oflags & NEWLIB_O_RDONLY) != 0)    ret_val |= O_RDONLY;
  if ((oflags & NEWLIB_O_WRONLY) != 0)    ret_val |= O_WRONLY;
  if ((oflags & NEWLIB_O_RDWR) != 0)      ret_val |= O_RDWR;
  if ((oflags & NEWLIB_O_APPEND) != 0)    ret_val |= O_APPEND;
  if ((oflags & NEWLIB_O_CREAT) != 0)     ret_val |= O_CREAT;
  if ((oflags & NEWLIB_O_TRUNC) != 0)     ret_val |= O_TRUNC;
  if ((oflags & NEWLIB_O_EXCL) != 0)      ret_val |= O_EXCL;
  if ((oflags & NEWLIB_O_SYNC) != 0)      ret_val |= O_SYNC;
  if ((oflags & NEWLIB_O_NONBLOCK) != 0)  ret_val |= O_NONBLOCK;
  if ((oflags & NEWLIB_O_NOCTTY) != 0)    ret_val |= O_NOCTTY;
  if ((oflags & NEWLIB_O_CLOEXEC) != 0)   ret_val |= O_CLOEXEC;
  if ((oflags & NEWLIB_O_NOFOLLOW) != 0)  ret_val |= O_NOFOLLOW;
  if ((oflags & NEWLIB_O_DIRECTORY) != 0) ret_val |= O_DIRECTORY;
  return ret_val;
}

// These defines are repeated in brew-decode.c. Let's keep the in sync!
#define SIM_REG_T(idx) sim_state->dirty_map |= (1<<(idx)); sim_state->reg[idx]
#define MAKE_INT(l0, type) (brew_typed_reg) { l0, type }

#define SET_ERRNO(value) { SIM_REG_T(BREW_REG_SYSCALL_ERRNO) = MAKE_INT(value, BREW_REG_TYPE_INT32); }


// Copies a string from SIM memory to host memory.
// Returns a pointer to the string, WHICH NEEDS TO BE FREE-d
static char *sim_core_read_str(sim_cpu *scpu, uint32_t vma)
{
  size_t alloc_size = 1024;
  char *str = (char *)malloc(alloc_size);
  size_t str_len = 0;
  char *c = str;
  SIM_DESC sd = CPU_STATE(scpu);

  SIM_ASSERT(str != NULL);
  do
    {
      *c = sim_core_read_1(scpu, CPU_PC_GET(scpu), read_map, vma);
      if (*c == 0)
        break;
      ++c;
      ++str_len;
      ++vma;
      if (str_len >= alloc_size)
        {
          alloc_size *= 2;
          str = (char *)realloc(str, alloc_size);
          SIM_ASSERT(str != NULL);
          c = str+str_len;
        }
    } while (true);
  return str;
}


static void user_mode_handle_syscall(SIM_DESC sd, sim_cpu *scpu)
{
  // Syscalls follow normal function-call ABI with the addition that
  // $r3 points to 'errno' on entry.
  // The ABI declares that $r8...$r14 needs to maintain it's value
  // accross calls. This applies to SYSCALLs too. $sp and $fp need
  // to be preserved as well. $r3...$r7 can change, though it's quite
  // possible that a true executive implementaton would preserve those
  // as well. In the simulator, we'll only override the return value
  // register(s) and preserve all other.
  //
  // The syscall number is held at the 16-bit address after the SYSCALL
  // instruction. This is the address where the PC points to in real HW,
  // but NOT where it points in the simulator.
  // $pc needs to be advanced by 4 prior returning from the SYSCALL
  // into task mode. Again, the simulator - being task-mode only for now -
  // only needs to worry about setting the PC up appropriately.
  //
  // NOTE: there's actually a generic syscall interface in the simulator framework.
  //       This is how it's called:
  //  /* XXX: We don't pass back the actual errno value.  */
  //  gr[RET1] = sim_syscall (cpu, gr[TRAPCODE], gr[PARM1], gr[PARM2], gr[PARM3], gr[PARM4]);
  //
  // I'm not sure how it handles mapping of structures and constant between host
  // and sim. It probably assumes they're the same, which is incorrect with newlib.
  // So, for now, I'll leave this code here.

  brew_sim_state *sim_state = &scpu->sim_state;

  uint16_t syscall_no;
  uint32_t arg1 = sim_state->reg[BREW_REG_ARG0].val;
  uint32_t arg2 = sim_state->reg[BREW_REG_ARG1].val;
  uint32_t arg3 = sim_state->reg[BREW_REG_ARG2].val;
  uint32_t arg4 = sim_state->reg[BREW_REG_ARG3].val;
  char *str1;
  char *str2;
  int ret_val;
  int flags;

  syscall_no = sim_core_read_aligned_2(scpu, sim_state->tpc, exec_map, sim_state->tpc+2);
  sim_state->ntpc = sim_state->tpc + 4; // skip over syscall insn as well as the syscal number before returning.
  switch (syscall_no) {
    case SYS_exit:
      // simulated process terminating: let's terminate too!
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_exited, arg1);
      //exit(arg1);
      break;
    case SYS_open:
    {
      uint32_t mode;
      str1 = sim_core_read_str(scpu, arg1);
      // We need to map some flags
      flags = marshal_o_flags_from_sim(arg2);
      // mode is a VAARG argument. Even so, those come through registers as well, just as regular arguments.
      mode = arg3;

      fflush(stdout);

      //printf("SIM OPEN: %s with flags: %x, local flags are: %x and mode 0%o\n", str1, arg2, flags, mode);
      ret_val = open(str1, flags, mode);
      //printf("SIM OPEN returned: %d, errno: %d\n", ret_val, errno);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    }
    case SYS_close:
      if (arg1 > 2)
        ret_val = close(arg1);
      else
        ret_val = 0;
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_read: {
      void *buffer = malloc(arg3);
      ret_val = read(arg1, buffer, arg3);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val >= 0)
        sim_core_write_buffer(sd, scpu, write_map, buffer, arg2, arg3);
      else
        SET_ERRNO(errno);
      free(buffer);
      break;
    }
    case SYS_write: {
      void *buffer = malloc(arg3);
      sim_core_read_buffer(sd, scpu, write_map, buffer, arg2, arg3);
      ret_val = write(arg1, buffer, arg3);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      free(buffer);
      break;
    }
    case SYS_lseek:
      ret_val = lseek(arg1, arg2, arg3);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_unlink:
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = unlink(str1);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_getpid:
      // Since we don't simulate multiple processes, we simply return the PID of the simulator.
      ret_val = getpid();
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      break;
#if ! defined __MINGW32__
    case SYS_kill:
      ret_val = kill(arg1, arg2);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
#endif // __MINGW32__
    case SYS_fstat: {
      struct stat statbuf;
      ret_val = fstat(arg1, &statbuf);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      // for now, simply return an error...
      ret_val = -1;
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      SET_ERRNO(EOVERFLOW);
      //SIM_ASSERT(false);
      break;
    }

    // These are not implemented
    case SYS_argvlen:
    case SYS_argv:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_stopped, SIM_SIGTRAP);
      break;
    case SYS_chdir:
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = chdir(str1);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_stat: {
      struct stat statbuf;
      ret_val = fstat(arg1, &statbuf);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
    case SYS_chmod:
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = chmod(str1, arg2);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_utime: {
      struct utimbuf times;
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = utime(str1, &times);
      free(str1);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
    case SYS_time: {
      time_t now = time(NULL);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)now, BREW_REG_TYPE_INT32);
      if (arg1 != 0)
        {
          sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, arg1, now);
        }
      break;
    }
    case SYS_gettimeofday: {
      struct timeval time_val;
      struct timezone time_zone;
      ret_val = gettimeofday(&time_val, &time_zone);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
#if ! defined __MINGW32__
    case SYS_times: {
      struct tms time_buf;
      ret_val = times(&time_buf);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
#endif // __MINGW32__
#if ! defined __MINGW32__
    case SYS_link:
      str1 = sim_core_read_str(scpu, arg1);
      str2 = sim_core_read_str(scpu, arg2);
      ret_val = link(str1, str2);
      free(str1);
      free(str2);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
#endif // __MINGW32__

    // These are not implemented
    case SYS_argc:
    case SYS_argnlen:
    case SYS_argn:
    case SYS_reconfig:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_stopped, SIM_SIGTRAP);
      break;
    // Brew-specific calls:
    case BREW_isatty:
      ret_val = isatty(arg1);
      SIM_REG_T(BREW_REG_ARG0) = MAKE_INT((uint32_t)ret_val, BREW_REG_TYPE_INT32);
      if (ret_val == 0)
        SET_ERRNO(errno);
      break;

    default:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_stopped, SIM_SIGTRAP);
      break;
  }
}




static void check_alignment(sim_cpu *scpu, uint32_t addr, int length)
{
  brew_sim_state *sim_state = &scpu->sim_state;
  SIM_DESC sd = CPU_STATE(scpu);

  switch (length) {
    case 8:
    return;
    case 16:
      if ((addr & 1) != 0)
        {
          sim_state->ecause = BREW_EXCEPTION_UNALIGNED;
          sim_state->csr_eaddr = addr;
        }
    return;
    case 32:
      if ((addr & 3) != 0)
        {
          sim_state->ecause = BREW_EXCEPTION_UNALIGNED;
          sim_state->csr_eaddr = addr;
        }
    return;
  }
  SIM_ASSERT(false);
}


static INLINE void record_mem_trace(sim_cpu *scpu, uint32_t addr, int access_size, bool is_write, uint32_t value)
{
  scpu->mem_trace.addr = addr;
  scpu->mem_trace.is_write = is_write;
  scpu->mem_trace.access_size = access_size;
  scpu->mem_trace.value = value;
  scpu->mem_trace.is_valid = true;
}

static INLINE void read_mem(sim_cpu *scpu, uint32_t vma, int length, uint32_t *value, v2p_addr_ftype v2p_addr)
{
  SIM_DESC sd = CPU_STATE(scpu);
  brew_sim_state *sim_state = &scpu->sim_state;
  uint32_t pma;

  check_alignment(scpu, vma, length);
  if (sim_state->ecause != BREW_EXCEPTION_NONE)
    return;

  v2p_addr(scpu, vma, acc_read, &pma);
  if (sim_state->ecause != BREW_EXCEPTION_NONE)
    return;

  switch (length) {
    case 8:  *value = sim_core_read_aligned_1(scpu, CPU_PC_GET(scpu), read_map, pma); break;
    case 16: *value = sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), read_map, pma); break;
    case 32: *value = sim_core_read_aligned_4(scpu, CPU_PC_GET(scpu), read_map, pma); break;
    default: SIM_ASSERT(false);
  }
  record_mem_trace(scpu, vma, length,  false, *value);
}

static INLINE uint32_t read_inst(sim_cpu *scpu, uint32_t vma, int length, v2p_addr_ftype v2p_addr)
{
  SIM_DESC sd = CPU_STATE(scpu);
  brew_sim_state *sim_state = &scpu->sim_state;
  uint32_t pma;
  uint32_t ret_val;

  // For instructions, we ignore the LSB --> this also means that there's no possibility for an unaligned exception
  vma &= ~1;

  v2p_addr(scpu, vma, acc_exec, &pma);
  if (sim_state->ecause != BREW_EXCEPTION_NONE)
    return 0;


  switch (length) {
    case 8:  ret_val = sim_core_read_aligned_1(scpu, CPU_PC_GET(scpu), exec_map, pma); break;
    case 16: ret_val = sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), exec_map, pma); break;
    case 32: ret_val = sim_core_read_unaligned_4(scpu, CPU_PC_GET(scpu), exec_map, pma); break;
    default: SIM_ASSERT(false);
  }
  record_mem_trace(scpu, vma, length,  false, ret_val);
  return ret_val;
}

static INLINE void write_mem(sim_cpu *scpu, uint32_t vma, int length, uint32_t value, v2p_addr_ftype v2p_addr)
{
  SIM_DESC sd = CPU_STATE(scpu);
  brew_sim_state *sim_state = &scpu->sim_state;
  uint32_t pma;

  check_alignment(scpu, vma, length);
  if (sim_state->ecause != BREW_EXCEPTION_NONE)
    return;

  v2p_addr(scpu, vma, acc_write, &pma);
  if (sim_state->ecause != BREW_EXCEPTION_NONE)
    return;

  record_mem_trace(scpu, vma, length, true, value);
  switch (length) {
    case 8:  sim_core_write_aligned_1(scpu, CPU_PC_GET(scpu), write_map, pma, value); return;
    case 16: sim_core_write_aligned_2(scpu, CPU_PC_GET(scpu), write_map, pma, value); return;
    case 32: sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, pma, value); return;
    default: SIM_ASSERT(false);
  }
}




static _Float16 rsqrt_fp16(_Float16 num)
{
  return 1.0f / sqrtf(num);
}

static float rsqrt(float num)
{
  return 1.0f / sqrtf(num);
}



/////////////////////////////////////////////////////////////////////////
// BREW MODEL
/////////////////////////////////////////////////////////////////////////

struct brew_model_info
{
};

static void brew_model_done(sim_cpu *scpu)
{
  free(scpu->model_info);
  printf("Done with model BREW\n");
}

static void brew_model_setup_sim(SIM_DESC sd, sim_cpu *scpu)
{
  // FIXME: not sure what these do. Probably create and allocate some memory
  sim_do_command(sd," memory region 0x00000000,0x80000000"); // main memory for program storage
}

static void brew_model_setup_sd(SIM_DESC sd ATTRIBUTE_UNUSED)
{
  sim_cpu *scpu = STATE_CPU(sd, 0); // It's a hack, but the ROM FW is always stored on the model_info structure of the first CPU

  const char *rom_fw = scpu->rom_file_name;
  if (rom_fw != NULL)
    {
      sim_io_eprintf(sd, "WARNING: rom-file %s ignored for brew models\n", rom_fw);
    }
}

static void brew_model_v2p_addr(sim_cpu *scpu, uint32_t v_addr, brew_access_modes access_mode, uint32_t *p_addr)
{
  SIM_DESC sd = CPU_STATE(scpu);

  SIM_ASSERT(p_addr != NULL);
  *p_addr = v_addr;
}

static void brew_model_read_mem(sim_cpu *scpu, uint32_t vma, int length, uint32_t *value)
{
  read_mem(scpu, vma, length, value, brew_model_v2p_addr);
}

static uint32_t brew_model_read_inst(sim_cpu *scpu, uint32_t vma, int length)
{
  return read_inst(scpu, vma, length, brew_model_v2p_addr);
}

static void brew_model_write_mem(sim_cpu *scpu, uint32_t vma, int length, uint32_t value)
{
  write_mem(scpu, vma, length, value, brew_model_v2p_addr);
}

static void brew_model_write_csr(sim_cpu *scpu, uint16_t addr, uint32_t data)
{
  SIM_DESC sd = CPU_STATE(scpu);
  sim_engine_abort(sd, scpu, NULL_CIA, "Unknown write at CSR address 0x%04x", addr);
}

static uint32_t brew_model_read_csr(sim_cpu *scpu, uint16_t addr)
{
  brew_sim_state *sim_state = &scpu->sim_state;
  SIM_DESC sd = CPU_STATE(scpu);
  if (sim_state->is_task_mode) addr |= 0x8000;
  switch (addr) {
    case BREW_CSR_MARCH:
      return 0;
    break;
    case BREW_CSR_CAPABILITY:
      return 0;
    break;
    case BREW_CSR_FPSTAT:
      sim_engine_abort(sd, scpu, NULL_CIA, "FIXME: brew model doesn't support CSR read of FPSTAT");
    break;
    case BREW_CSR_ECAUSE: {
      uint32_t ret_val = sim_state->csr_ecause;
      sim_state->csr_ecause = BREW_EXCEPTION_NONE;
      return ret_val;
    } break;
    case BREW_CSR_EADDR:
      return sim_state->csr_eaddr;
    break;
    default:
      sim_engine_abort(sd, scpu, NULL_CIA, "Unknown read at CSR address 0x%04x", addr);
    break;
  }
}

static void brew_model_handle_exception(sim_cpu *scpu)
{
  brew_sim_state *sim_state = &scpu->sim_state;
  SIM_DESC sd = CPU_STATE(scpu);
  // TODO: don't forget to set ntpc, nspc, nis_task_mode as appropriate
  if (!sim_state->is_task_mode)
    {
      sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGTRAP);
    }
  switch (STATE_ENVIRONMENT(sd))
    {
    case USER_ENVIRONMENT:
    case VIRTUAL_ENVIRONMENT:
      // All but the SYSCALL exception are fatal in user-mode sim
      switch(sim_state->ecause)
        {
        case BREW_EXCEPTION_HWI:               sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_FILL:              sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_BREAK:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGTRAP); break;
        case BREW_EXCEPTION_SYSCALL:           user_mode_handle_syscall(sd, scpu); break;
        case BREW_EXCEPTION_SWI_3:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_SWI_4:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_SWI_5:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_SWI_6:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_SWI_7:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_UNKNOWN_INST:      sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_TYPE:              sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
        case BREW_EXCEPTION_UNALIGNED:         sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGSEGV); break;
        default:                               sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGNONE); break;
        }
      break;
    case OPERATING_ENVIRONMENT:
      // In system mode simulations, we simply switch to scheduler mode
      sim_state->nis_task_mode = false;
      break;
    default:
      SIM_ASSERT(false);
    }
}

static void brew_model_handle_interrupt(sim_cpu *scpu)
{
}

static void brew_model_reset_cpu(sim_cpu *scpu, bool is_user_mode_sim)
{
  brew_sim_state *sim_state = &scpu->sim_state;

  sim_state->dirty_map = 0;
  sim_state->spc = 0;
  sim_state->tpc = 0;
  sim_state->reg[BREW_REG_SP] = MAKE_INT(0x01000000, BREW_REG_TYPE_INT32); // Setting $sp to something meaningful
  sim_state->is_task_mode = is_user_mode_sim;
  /* FIXME: do we want to get some other state in the registers? */
  /* Such as:
     - explicit randomization
     - results of POST
     - version/revision info
  */
}

void brew_model_init(sim_cpu *scpu)
{
  SIM_DESC sd = CPU_STATE(scpu);
  int i;

  printf("Setting model to BREW\n");

  scpu->model_info = ZALLOC(brew_model_info);
  brew_model_functions *model_functions = &scpu->sim_state.model_functions;
  model_functions->setup_sim = brew_model_setup_sim;
  model_functions->setup_sd = brew_model_setup_sd;
  model_functions->free_model_info = brew_model_done;
  model_functions->read_mem = brew_model_read_mem;
  model_functions->read_inst = brew_model_read_inst;
  model_functions->write_mem = brew_model_write_mem;
  model_functions->write_csr = brew_model_write_csr;
  model_functions->read_csr = brew_model_read_csr;
  model_functions->handle_exception = brew_model_handle_exception;
  model_functions->reset_cpu = brew_model_reset_cpu;
  model_functions->handle_interrupt = brew_model_handle_interrupt;
  model_functions->rsqrt_fp16 = rsqrt_fp16;
  model_functions->rsqrt = rsqrt;

  // Let's try to set up the simulator. We can only do that, if the 'sd' member is populated
  // already, which only happens if the model is set as the default, not on the command line.
  // If the model is specified on the command line, we'll have to defer the call to setup_sim
  // to sim_open in interp.c
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      sim_cpu *scpu = STATE_CPU (sd, i);

      scpu->sim_state.model_functions.setup_sim(sd, scpu);
    }
}



/////////////////////////////////////////////////////////////////////////
// ESPRESSO MODEL
/////////////////////////////////////////////////////////////////////////

static const uint32_t ESPRESSO_ROM_BASE        = 0x00000000;
static const uint32_t ESPRESSO_ROM_SIZE        = 64*1024;
static const uint32_t ESPRESSO_ROM_DECODE_SIZE = 0x00010000;
static const uint32_t ESPRESSO_RAM_BASE        = 0x08000000;
static const uint32_t ESPRESSO_RAM_SIZE        = 1*1024*1024;
static const uint32_t ESPRESSO_RAM_DECODE_SIZE = 0x04000000;

struct espresso_model_info: public brew_model_info {
  uint32_t pmem_base;
  uint32_t pmem_limit;
  uint32_t dmem_base;
  uint32_t dmem_limit;
  char *rom_buffer;
  char *ram_buffer;
  char *rom_fw_name;
};

static void espresso_model_done(sim_cpu *scpu)
{
  SIM_DESC sd = CPU_STATE(scpu);
  uint32_t wait_state;

  printf("Done with model ESPRESSO\n");
  SIM_ASSERT(scpu->model_info != NULL);
  espresso_model_info *model_info = (espresso_model_info*)scpu->model_info;

  // detach the ROM and RAM memories to all aliased locations.
  if (model_info->rom_buffer != NULL) {
    for (wait_state=0;wait_state<=15;++wait_state) {
      uint32_t rom_base = ESPRESSO_ROM_BASE + (wait_state << 28);
      sim_core_detach(
        sd,
        NULL,
        0,
        0, // address_space: not sure what this is
        rom_base
      );
    }
    free(model_info->rom_buffer);
    model_info->rom_buffer = NULL;
  }

  if (model_info->ram_buffer != NULL) {
    for (wait_state=0;wait_state<=15;++wait_state) {
      uint32_t ram_base = ESPRESSO_RAM_BASE + (wait_state << 28);
      sim_core_detach(
        sd,
        NULL,
        0,
        0, // address_space: not sure what this is
        ram_base
      );
    }
    free(model_info->ram_buffer);
    model_info->ram_buffer = NULL;
  }

  if (model_info->rom_fw_name != NULL) {
    free(model_info->rom_fw_name);
    model_info->rom_fw_name = NULL;
  }

  free(scpu->model_info);
  scpu->model_info = NULL;
}

// Returns a 64-bit aligned buffer.
static char* align_alloc(size_t size)
{
  size_t alloc_size = ((size + 7) / 8) * 8;
  return (char*)zalloc(alloc_size);
}

static void espresso_model_setup_sim(SIM_DESC sd, sim_cpu *scpu)
{
  int32_t wait_state;
  espresso_model_info *model_info = (espresso_model_info*)scpu->model_info;

  //char buf[256];
  //snprintf(buf, sizeof(buf), " memory region 0x%08x,0x%08x", ESPRESSO_ROM_BASE, ESPRESSO_ROM_SIZE);
  //sim_do_command(sd, buf);
  //snprintf(buf, sizeof(buf), " memory region 0x%08x,0x%08x", ESPRESSO_RAM_BASE, ESPRESSO_RAM_SIZE);
  //sim_do_command(sd, buf);

  // Attach the ROM and RAM memories to all aliased locations.
  model_info->rom_buffer = align_alloc(ESPRESSO_ROM_SIZE);
  for (wait_state=0;wait_state<=15;++wait_state) {
    uint32_t rom_base = ESPRESSO_ROM_BASE + (wait_state << 28);
    sim_core_attach(
      sd,
      NULL,
      0,
      access_read_write_exec, // For now, allow writes into ROM as well. Maybe later change to read-only???
      0, // address_space: not sure what this is
      rom_base,
      ESPRESSO_ROM_DECODE_SIZE,
      ESPRESSO_ROM_SIZE,
      NULL, // client HW (struct hw *)
      model_info->rom_buffer // buffer
    );
  }

  model_info->ram_buffer = align_alloc(ESPRESSO_RAM_SIZE);
  for (wait_state=0;wait_state<=15;++wait_state) {
    uint32_t ram_base = ESPRESSO_RAM_BASE + (wait_state << 28);
    sim_core_attach(
      sd,
      NULL,
      0,
      access_read_write_exec,
      0, // address_space: not sure what this is
      ram_base,
      ESPRESSO_RAM_DECODE_SIZE,
      ESPRESSO_RAM_SIZE,
      NULL, // client HW (struct hw *)
      model_info->ram_buffer // buffer
    );
  }

  /*
  This is very poorly documented, but here's what I gathered so far:

  Devices participate in a hierarchy, normally starting from 'core'. The last entry in the path is usually a 'reg' entry, which
  lists the base/length pairs of the register entries that are assigned to the device.

  The device creation actually happens as the core walks the path, so really what's going on is that the device gets created because
  there is a 'reg' entry for it.

  At any rate, the reg entry gets stored in the 'hw' structure for the device. The number of base/size pairs is only implicitly stored,
  the device needs to know how many to expect (although the corresponding query function `hw_find_reg_array_property` errs out with
  return code 0 if an incorrect index is used, so in theory the detection of the number of entries could be made dynamic).

  The actual registration of these addresses is the responsibility of the device; it is not done automatically.

  Register addresses are also going through address translation in the hierarchy, something I don't use (thus understand) completely
  at the moment.

  Since device creation happens during the walk of the path, multiple instances of devices is not trivial to create.

  One mechanism is to provide a different name then the family. The family is provided in parenthesis. So,
     /core/foo/reg
  creates a foo sub-device with name='foo' and family='foo' as well.
     /core/(foo)bar/reg
  creates a foo sub-device with name='bar' and family='foo'.

  It is the 'family' that determines what device model gets used, and this needs to be one of the recognized names (dv-...).
  The name can be whatever. Also, if family is provided explicitly, it must precede the name. So `(foo)bar` is valid,
  while `bar(foo)` is not.

  There are other options that can be specified in the path to a device:

  /core/foo@unit/reg: would create a 'unit'. This will simply get passed to a callback on the 'bus' device's to_unit_decode.
    This is normally set to 'generic_hw_unit_decode'. The normal use is to create different instances of the same device
    at different base-addresses, which is what I'm using it for as well.

  /core/foo:args/reg: would create 'arguments': Whatever follows gets copied into hw->args_of_hw as a string, which then can
    be accessed through the 'hw_args' macro. I'm using it to pass an argument to the GPIO model, such that when it is written,
    it terminates the simulation. This is in accordance with the RTL simulator.

  Apart from register space definitions, one could also make 'port connections'. These (I think) are useful for interrupt
  or DMA routing. It takes the form: "/core/source_dev > rx   src_rx   /core/target_dev"

  */
  sim_hw_parse(sd, "/core/brew_con@%#x/reg %#x %i",                     ANACHRON_UART_BASE,     ANACHRON_UART_BASE,     ANACHRON_UART_SIZE);
  sim_hw_parse(sd, "/core/brew_gpio@%#x/reg %#x %i",                    ANACHRON_GPIO_INT_BASE, ANACHRON_GPIO_INT_BASE, ANACHRON_GPIO_INT_SIZE);
  sim_hw_parse(sd, "/core/brew_gpio@%#x/reg %#x %i",                    ANACHRON_GPIO_3_BASE,   ANACHRON_GPIO_3_BASE,   ANACHRON_GPIO_3_SIZE);
  sim_hw_parse(sd, "/core/brew_gpio@%#x:terminate_on_write/reg %#x %i", ANACHRON_GPIO_4_BASE,   ANACHRON_GPIO_4_BASE,   ANACHRON_GPIO_4_SIZE);
}

// This is a routine that loads an binary into memory without modifying the bfd-related structures in 'sd'
// It is based on sim_load and sim_analyze_program, but gutted to keep state local
static SIM_RC load_binary(SIM_DESC sd, const char *prog_name)
{
  bfd *prog_bfd;

  SIM_ASSERT(STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  prog_bfd = sim_load_file(
    sd,
    STATE_MY_NAME(sd),                       // Name to print in error messages
    STATE_CALLBACK(sd),                      // stream writer for error messages
    prog_name,                               // name of the program to load
    NULL,                                    // existing BFD if already opened
    STATE_OPEN_KIND(sd) == SIM_OPEN_DEBUG,   // whether to do verbose tracing
    STATE_LOAD_AT_LMA_P(sd),                 // whether to load to LMA or VMA address
    sim_write                                // callback to actually write sections to memory
  );
  if (prog_bfd == NULL)
    return SIM_RC_FAIL;
  bfd_close(prog_bfd);

  return SIM_RC_OK;
}

static void espresso_model_setup_sd(SIM_DESC sd)
{
  sim_cpu *scpu = STATE_CPU(sd, 0); // It's a hack, but the ROM FW is always stored on the model_info structure of the first CPU

  // Try to load ROM firmware
  const char *rom_fw = scpu->rom_file_name;
  if (rom_fw != NULL)
    {
      if (load_binary(sd, rom_fw) != SIM_RC_OK)
        {
          sim_io_eprintf(sd, "WARNING: can't load ROM firmware from file %s\n", rom_fw);
        }
    }
}



static void espresso_model_v2p_addr(sim_cpu *scpu, uint32_t v_addr, brew_access_modes access_mode, uint32_t *p_addr)
{
  SIM_DESC sd = CPU_STATE(scpu);

  SIM_ASSERT(p_addr != NULL);

  espresso_model_info *model_info = (espresso_model_info *)(scpu->model_info);

  brew_sim_state *sim_state = &scpu->sim_state;
  // mask out wait-state (we don't care during simulation)
  const uint32_t ADDR_MASK = 0x0fffffff;
  if (!sim_state->is_task_mode) {
    *p_addr = v_addr & ADDR_MASK;
    return;
  }
  uint32_t base = model_info->dmem_base;
  uint32_t limit = model_info->dmem_limit;
  brew_exception_type exc = ESPRESSO_EXCEPTION_MEM_AV;
  if (access_mode == acc_exec) {
    base = model_info->pmem_base;
    limit = model_info->pmem_limit;
    exc = ESPRESSO_EXCEPTION_INST_AV;
  }
  if ((v_addr & ADDR_MASK & ~1023) > (limit & ADDR_MASK & ~1023)) {
    sim_state->ecause = exc;
    return;
  }
  *p_addr = ((v_addr & ADDR_MASK) + (base & ADDR_MASK & ~1023)) & ADDR_MASK;
}

static void espresso_model_read_mem(sim_cpu *scpu, uint32_t vma, int length, uint32_t *value)
{
  read_mem(scpu, vma, length, value, espresso_model_v2p_addr);
}

static uint32_t espresso_model_read_inst(sim_cpu *scpu, uint32_t vma, int length)
{
  return read_inst(scpu, vma, length, espresso_model_v2p_addr);
}

static void espresso_model_write_mem(sim_cpu *scpu, uint32_t vma, int length, uint32_t value)
{
  write_mem(scpu, vma, length, value, espresso_model_v2p_addr);
}


static void espresso_model_write_csr(sim_cpu *scpu, uint16_t addr, uint32_t data)
{
  SIM_DESC sd = CPU_STATE(scpu);
  espresso_model_info *model_info = (espresso_model_info *)(scpu->model_info);
  switch (addr) {
    case ESPRESSO_CSR_PMEM_BASE_REG:  model_info->pmem_base  = data & (0x0fffffff) & ~1023; break;
    case ESPRESSO_CSR_PMEM_LIMIT_REG: model_info->pmem_limit = data & (0x0fffffff) & ~1023; break;
    case ESPRESSO_CSR_DMEM_BASE_REG:  model_info->dmem_base  = data & (0x0fffffff) & ~1023; break;
    case ESPRESSO_CSR_DMEM_LIMIT_REG: model_info->dmem_limit = data & (0x0fffffff) & ~1023; break;
    default:
      brew_model_write_csr(scpu, addr, data);
    break;
  }
}

static uint32_t espresso_model_read_csr(sim_cpu *scpu, uint16_t addr)
{
  SIM_DESC sd = CPU_STATE(scpu);
  espresso_model_info *model_info = (espresso_model_info *)(scpu->model_info);
  switch (addr) {
    case ESPRESSO_CSR_PMEM_BASE_REG:  return model_info->pmem_base  & (0x0fffffff) & ~1023;
    case ESPRESSO_CSR_PMEM_LIMIT_REG: return model_info->pmem_limit & (0x0fffffff) & ~1023;
    case ESPRESSO_CSR_DMEM_BASE_REG:  return model_info->dmem_base  & (0x0fffffff) & ~1023;
    case ESPRESSO_CSR_DMEM_LIMIT_REG: return model_info->dmem_limit & (0x0fffffff) & ~1023;
    default: return brew_model_read_csr(scpu, addr);
  }
  SIM_ASSERT(false);
  return 0;
}

static void espresso_model_reset_cpu(sim_cpu *scpu, bool is_user_mode_sim)
{
  espresso_model_info *model_info = (espresso_model_info *)(scpu->model_info);

  brew_model_reset_cpu(scpu, is_user_mode_sim);

  model_info->pmem_base = 0;
  model_info->dmem_base = 0;
  if (is_user_mode_sim)
    {
      // Setting $sp and limit registers to a minimally useful environment
      model_info->pmem_limit = 0x0ffffc00;
      model_info->dmem_limit = 0x0ffffc00;
      scpu->sim_state.reg[BREW_REG_SP] = MAKE_INT(ESPRESSO_RAM_BASE + ESPRESSO_RAM_SIZE, BREW_REG_TYPE_INT32);
    }
  else
    {
      model_info->pmem_limit = 0;
      model_info->dmem_limit = 0;
    }
}

static void espresso_model_handle_exception(sim_cpu *scpu)
{
  brew_model_handle_exception(scpu);
//  brew_sim_state *sim_state = &scpu->sim_state;
//  SIM_DESC sd = CPU_STATE(scpu);
//  // TODO: don't forget to set ntpc, nspc, nis_task_mode as appropriate
//  if (!sim_state->is_task_mode)
//    {
//      sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGTRAP);
//    }
//  switch (STATE_ENVIRONMENT(sd))
//    {
//    case USER_ENVIRONMENT:
//    case VIRTUAL_ENVIRONMENT:
//      // All but the SYSCALL exception are fatal in user-mode sim
//      switch(sim_state->ecause)
//        {
//        case BREW_EXCEPTION_HWI:               sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_FILL:              sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_BREAK:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGTRAP); break;
//        case BREW_EXCEPTION_SYSCALL:           user_mode_handle_syscall(sd, scpu); break;
//        case BREW_EXCEPTION_SWI_3:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_SWI_4:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_SWI_5:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_SWI_6:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_SWI_7:             sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_UNKNOWN_INST:      sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_TYPE:              sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGILL); break;
//        case BREW_EXCEPTION_UNALIGNED:         sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGSEGV); break;
//        default:                               sim_engine_halt(sd, scpu, NULL, sim_state->tpc, sim_stopped, SIM_SIGNONE); break;
//        }
//      break;
//    case OPERATING_ENVIRONMENT:
//      // In system mode simulations, we simply switch to scheduler mode
//      sim_state->nis_task_mode = false;
//    default:
//      SIM_ASSERT(false);
//    }
}

static void espresso_model_handle_interrupt(sim_cpu *scpu)
{
}

void espresso_model_init(sim_cpu *scpu)
{
  SIM_DESC sd = CPU_STATE(scpu);
  int i;

  printf("Setting model to ESPRESSO\n");

  espresso_model_info *model_info = ZALLOC(espresso_model_info);
  scpu->model_info = model_info;
  model_info->pmem_base = 0;
  model_info->pmem_limit = 0;
  model_info->dmem_base = 0;
  model_info->dmem_limit = 0;
  model_info->rom_buffer = NULL;
  model_info->ram_buffer = NULL;

  brew_model_functions *model_functions = &scpu->sim_state.model_functions;
  model_functions->setup_sim = espresso_model_setup_sim;
  model_functions->setup_sd = espresso_model_setup_sd;
  model_functions->free_model_info = espresso_model_done;
  model_functions->read_mem = espresso_model_read_mem;
  model_functions->read_inst = espresso_model_read_inst;
  model_functions->write_mem = espresso_model_write_mem;
  model_functions->write_csr = espresso_model_write_csr;
  model_functions->read_csr = espresso_model_read_csr;
  model_functions->handle_exception = espresso_model_handle_exception;
  model_functions->reset_cpu = espresso_model_reset_cpu;
  model_functions->handle_interrupt = espresso_model_handle_interrupt;
  model_functions->rsqrt_fp16 = rsqrt_fp16;
  model_functions->rsqrt = rsqrt;

  // Let's try to set up the simulator. We can only do that, if the 'sd' member is populated
  // already, which only happens if the model is set as the default, not on the command line.
  // If the model is specified on the command line, we'll have to defer the call to setup_sim
  // to sim_open in interp.c
  if (sd != NULL)
    {
      for (i = 0; i < MAX_NR_PROCESSORS; ++i)
        {
          sim_cpu *scpu = STATE_CPU (sd, i);
          scpu->sim_state.model_functions.setup_sim(sd, scpu);
        }
    }
}





// Create command-line options to define various aspects of the simulation

static SIM_RC brew_option_handler(SIM_DESC sd, sim_cpu *scpu, int opt, char *arg, int is_command)
{
  switch (opt)
    {
    case OPTION_ESPRESSO_ROM_FILE:
      if (arg == NULL) {
        sim_io_eprintf(sd, "FIXME: default ROM file is not implemented yet for --rom-file option\n");
        return SIM_RC_FAIL;
      }
      STATE_CPU(sd, 0)->rom_file_name = strdup(arg);
      return SIM_RC_OK;

    default:
      sim_io_eprintf(sd, "Unknown option %d\n", opt);
      return SIM_RC_FAIL;
    }
}

static const OPTION brew_options[] =
{
  {
    {"rom-file", optional_argument, NULL, OPTION_ESPRESSO_ROM_FILE },
    '\0',
    "FILE NAME",
    "Specify ROM firmware to load before executable. If not specified, no firmware is loaded.",
    brew_option_handler,
    NULL
  },

  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL, NULL }
};

SIM_RC brew_options_init(SIM_DESC sd)
{
  SIM_ASSERT(STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (sim_add_option_table (sd, NULL, brew_options) != SIM_RC_OK)
    return SIM_RC_FAIL;
  //sim_module_add_init_fn (sd, dv_sockser_init);
  //sim_module_add_uninstall_fn (sd, dv_sockser_uninstall);
  return SIM_RC_OK;
}
