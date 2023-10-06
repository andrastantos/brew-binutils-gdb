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

#include "dv-con.h"

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


static const uint32_t ANACHRON_IO_APB_BASE      = 0x00020000;

static const uint32_t ANACHRON_UART_BASE        = ANACHRON_IO_APB_BASE + 0x0000;
static const uint32_t ANACHRON_UART_SIZE        = 5;


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


struct brew_model_info {
public:
  brew_model_info() {
    printf("Setting model to BREW\n");
  }
  virtual ~brew_model_info() {}

  virtual void setup_sim(SIM_DESC sd) {
    // FIXME: not sure what these do. Probably create and allocate some memory
    sim_do_command(sd," memory region 0x00000000,0x80000000"); // main memory for program storage
  }
  virtual void v2p_addr(sim_cpu *scpu, uint32_t v_addr, brew_access_modes access_mode, uint32_t &p_addr) { p_addr = v_addr; }
  virtual void write_csr(sim_cpu *scpu, uint16_t addr, uint32_t data) {
    SIM_DESC sd = CPU_STATE(scpu);
    sim_engine_abort(sd, scpu, NULL_CIA, "FIXME: brew model doesn't support CSR writes");
  }
  virtual uint32_t read_csr(sim_cpu *scpu, uint16_t addr) {
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
  virtual void handle_exception(sim_cpu *scpu) {
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
      default:
        SIM_ASSERT(false);
      }
  }
  virtual void handle_interrupt(sim_cpu *scpu) {}
};

struct espresso_model_info: public brew_model_info {
public:
  static const uint32_t ROM_BASE = 0x00000000;
  static const uint32_t ROM_SIZE = 64*1024;
  static const uint32_t RAM_BASE = 0x80000000;
  static const uint32_t RAM_SIZE = 1*1024*1024;

  uint32_t pmem_base;
  uint32_t pmem_limit;
  uint32_t dmem_base;
  uint32_t dmem_limit;

  espresso_model_info() {
    printf("Setting model to ESPRESSO\n");
    pmem_base = 0;
    pmem_limit = 0;
    dmem_base = 0;
    dmem_limit = 0;
  }
  virtual void setup_sim(SIM_DESC sd) {
    char buf[256];
    snprintf(buf, sizeof(buf), " memory region 0x%08x, 0x%08x", ROM_BASE, ROM_SIZE);
    sim_do_command(sd, buf);
    snprintf(buf, sizeof(buf), " memory region 0x%08x, 0x%08x", RAM_BASE, RAM_SIZE);
    sim_do_command(sd, buf);

    sim_hw_parse(sd, "/core/%s/reg %#x %i", "brew_con", ANACHRON_UART_BASE, ANACHRON_UART_SIZE);
  }
  virtual void v2p_addr(sim_cpu *scpu, uint32_t v_addr, brew_access_modes access_mode, uint32_t &p_addr) {
    brew_sim_state *sim_state = &scpu->sim_state;
    // mask out wait-state (we don't care during simulation)
    const uint32_t ADDR_MASK = 0x0fffffff;
    if (!sim_state->is_task_mode) {
      p_addr = v_addr & ADDR_MASK;
      return;
    }
    uint32_t base = dmem_base;
    uint32_t limit = dmem_limit;
    brew_exception_type exc = ESPRESSO_EXCEPTION_MEM_AV;
    if (access_mode == acc_exec) {
      base = pmem_base;
      limit = pmem_limit;
      exc = ESPRESSO_EXCEPTION_INST_AV;
    }
    if ((v_addr & ADDR_MASK & ~1023) > (limit & ADDR_MASK & ~1023)) {
      sim_state->ecause = exc;
      return;
    }
    p_addr = ((v_addr & ADDR_MASK) + (base & ADDR_MASK & ~1023)) & ADDR_MASK;
  }
};

struct anachron_model_info: public espresso_model_info {
public:
  anachron_model_info() {
    printf("Setting model to ANACHRON\n");
  }
};

static brew_model_info *get_model_info(sim_cpu *scpu)
{
  return (brew_model_info *)(scpu->model_info);
}

static void free_model_info(sim_cpu *scpu)
{
  delete get_model_info(scpu);
  scpu->model_info = nullptr;
}

void brew_model_init(sim_cpu *scpu)
{
  scpu->model_info = new brew_model_info();
  scpu->free_model_info = free_model_info;
}

void espresso_model_init(sim_cpu *scpu)
{
  scpu->model_info = new espresso_model_info();
  scpu->free_model_info = free_model_info;
}

void anachron_model_init(sim_cpu *scpu)
{
  scpu->model_info = new anachron_model_info();
  scpu->free_model_info = free_model_info;
}

void brew_model_setup_sim(sim_cpu *scpu, SIM_DESC sd) {
  brew_model_info *model_info = (brew_model_info*)(scpu->model_info);
  return model_info->setup_sim(sd);
}

void brew_model_v2p_addr(sim_cpu *scpu, uint32_t v_addr, brew_access_modes access_mode, uint32_t *p_addr) {
  SIM_DESC sd = CPU_STATE(scpu);
  SIM_ASSERT(p_addr != NULL);
  brew_model_info *model_info = (brew_model_info*)(scpu->model_info);
  model_info->v2p_addr(scpu, v_addr, access_mode, *p_addr);
}

uint32_t brew_model_read_csr(sim_cpu *scpu, uint16_t addr) {
  brew_model_info *model_info = (brew_model_info*)(scpu->model_info);
  return model_info->read_csr(scpu, addr);
}

void brew_model_write_csr(sim_cpu *scpu, uint16_t addr, uint32_t data) {
  brew_model_info *model_info = (brew_model_info*)(scpu->model_info);
  return model_info->write_csr(scpu, addr, data);
}

void brew_model_handle_exception(sim_cpu *scpu) {
  brew_model_info *model_info = (brew_model_info*)(scpu->model_info);
  return model_info->handle_exception(scpu);
}

void brew_model_handle_interrupt(sim_cpu *scpu) {
  brew_model_info *model_info = (brew_model_info*)(scpu->model_info);
  return model_info->handle_interrupt(scpu);
}


void brew_model_reset_cpu(sim_cpu *scpu, bool is_user_mode_sim)
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