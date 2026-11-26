/* Cycle model of the Espresso core (--timing=espresso).

This file is part of GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* This is a port of the control logic of the Espresso RTL
   (anachron/rtl/espresso: fetch.py, decode.py, reg_file.py, execute.py,
   memory.py, bus_if.py), register by register: every function below names
   the RTL module it follows, and the signal names are the RTL's.  The data
   path is left out.  The model runs beside the functional simulator, which
   tells it, in program order, which instructions executed, whether they
   branched and where they accessed memory; everything else (instruction
   lengths, register reads and reservations, execution units) the model
   decodes itself from the instruction words, with the RTL's decode table
   (espresso-decode.def), so that instructions fetched and decoded on the
   wrong path behind a taken branch cost what they cost on the core.

   Each cycle first evaluates the combinational signals in dependency order
   (requests to the bus interface, its arbitration and ready signals, the
   memory unit, execute stage 2 and 1, the register file, decode, instruction
   assembly, the instruction queue, the instruction buffer) and then commits
   the next values of all registers at once, as a clock edge would.

   Not modelled: DMA, interrupts, exceptions (they are timed as taken
   branches), CSR wait states (CSR accesses take the APB timing with no wait),
   instruction fetch access violations, and the multiplier/shifter-less
   configurations.  In the RTL, 'Select (c, a, b)' is 'c ? b : a'; the
   comments quote RTL expressions in that form where it matters.

   Checked against the RTL (its test rig, anachron/rtl/espresso/test/rig.py,
   with --model espresso --environment operating): the cycle every
   instruction entered execute agrees exactly for the timing
   microbenchmarks, random instruction mixes and compiled C kernels.
   The RTL before anachron 6b5d63b didn't take an indirect jump through
   memory ($pc <- mem32[...]) once the next instruction had entered execute
   stage 1: the jump is decided when its load returns, from stage 1's
   registers, which then held the next instruction.  The fix keeps the next
   instruction out of stage 1 until the jump is decided
   (indirect_branch_lockout), and so does this model.  Since anachron
   cfd2ad2, fetch starts no new burst while a load or store is between
   decode and the memory unit (mem_pending); before, a burst starting in
   those cycles made the access wait for all of it.  */

#include "espresso-timing.h"

#include <stdlib.h>
#include <string.h>

/* Decode table.  */

enum unit { U_ALU, U_SHIFT, U_MULT, U_BRANCH, U_BRANCH_IND, U_LD_ST, U_INVALID };
enum ldst { L_NONE, L_LOAD, L_STORE, L_CSR_LOAD, L_CSR_STORE };
enum mem_len { M_NONE, M_8, M_16, M_32 };
enum { PRED_EQ, PRED_NOT_F, PRED_ANY, PRED_LT, PRED_GT };
enum { REG_NONE = -1, REG_A = -2, REG_B = -3, REG_D = -4, REG_TINY_A = -5 };

typedef struct
{
  uint8_t pred_kind[4];   /* digits D, C, B, A */
  uint8_t pred_val[4];
  uint8_t unit, ldst, mem_len;
  int8_t rd1, rd2, res;
} decode_row;

#define P_EQ(v) PRED_EQ, v
#define P_NOT_F(v) PRED_NOT_F, v
#define P_ANY(v) PRED_ANY, v
#define P_LT(v) PRED_LT, v
#define P_GT(v) PRED_GT, v
#define R_NONE REG_NONE
#define R_A REG_A
#define R_B REG_B
#define R_D REG_D
#define R_TINY_A REG_TINY_A
#define R_CONST(n) (n)
#define ROW_PREDS(kd, vd, kc, vc, kb, vb, ka, va) { kd, kc, kb, ka }, { vd, vc, vb, va }
#define ROW_PREDS_EXPAND(...) ROW_PREDS (__VA_ARGS__)
#define ROW(comment, d, c, b, a, unit, ldst, rd1, rd2, res, len) \
  { ROW_PREDS_EXPAND (d, c, b, a), unit, ldst, len, rd1, rd2, res },

static const decode_row decode_rows[] =
{
#include "espresso-decode.def"
};

#undef ROW

typedef struct
{
  uint8_t unit, ldst, mem_len;
  int8_t rd1, rd2, res;   /* register numbers, -1 if not used */
} decoded;

static decoded decode_table[65536];
static bool decode_table_ready;

static bool
pred_match (int kind, int val, int digit)
{
  switch (kind)
    {
    case PRED_EQ: return digit == val;
    case PRED_NOT_F: return digit != 0xf;
    case PRED_ANY: return true;
    case PRED_LT: return digit < val;
    case PRED_GT: return digit > val && digit != 0xf;
    }
  return false;
}

static int8_t
reg_of (int8_t spec, uint16_t word)
{
  switch (spec)
    {
    case REG_NONE: return -1;
    case REG_A: return word & 0xf;
    case REG_B: return (word >> 4) & 0xf;
    case REG_D: return (word >> 12) & 0xf;
    case REG_TINY_A: return 12 | (word & 1);
    default: return spec;
    }
}

static void
build_decode_table (void)
{
  if (decode_table_ready)
    return;
  for (unsigned w = 0; w < 65536; ++w)
    {
      int digits[4] = { (w >> 12) & 0xf, (w >> 8) & 0xf, (w >> 4) & 0xf, w & 0xf };
      decoded *d = &decode_table[w];
      /* No row: the RTL decodes an unknown instruction exception, which
         goes through the branch unit and reads no registers.  */
      d->unit = U_BRANCH;
      d->ldst = L_NONE;
      d->mem_len = M_NONE;
      d->rd1 = d->rd2 = d->res = -1;
      for (size_t r = 0; r < sizeof (decode_rows) / sizeof (decode_rows[0]); ++r)
        {
          const decode_row *row = &decode_rows[r];
          int i;
          for (i = 0; i < 4; ++i)
            if (!pred_match (row->pred_kind[i], row->pred_val[i], digits[i]))
              break;
          if (i < 4)
            continue;
          d->unit = row->unit;
          d->ldst = row->ldst;
          d->mem_len = row->mem_len;
          d->rd1 = reg_of (row->rd1, w);
          d->rd2 = reg_of (row->rd2, w);
          d->res = reg_of (row->res, w);
          break;
        }
    }
  decode_table_ready = true;
}

/* InstAssemble's inst_len: 1, 2 or 3 16-bit words.  */
static int
inst_words (uint16_t w)
{
  int d = (w >> 12) & 0xf, c = (w >> 8) & 0xf, b = (w >> 4) & 0xf, a = w & 0xf;
  bool multi = d == 0xf || (c == 0xf && (b != 0xf || a == 0xf)) || (c == 0xe && a == 0xf)
               || (c < 0xc && (b == 0xf || a == 0xf));
  bool is32 = d == 0xf || a != 0xf;
  return multi ? (is32 ? 2 : 3) : 1;
}

/* An instruction in the pipeline.  */
typedef struct
{
  uint32_t addr;           /* byte address */
  uint16_t word;           /* first word */
  uint8_t words;           /* length in words */
  decoded d;
  bool bound;              /* matched to an instruction of the functional simulator */
  espresso_timing_insn rec;
} token;

/* Bus interface (bus_if.py).  */
enum bus_state
{
  BS_IDLE, BS_FIRST, BS_MIDDLE, BS_PRECHARGE, BS_ND_FIRST, BS_ND_WAIT, BS_ND_DUAL,
  BS_ND_DUAL_FIRST, BS_ND_DUAL_WAIT, BS_ND_LAST, BS_REFRESH
};
enum port { PORT_FETCH, PORT_MEM, PORT_REFRESH };

enum asm_state { AS_HAVE_0, AS_NEED_1, AS_NEED_2, AS_HAVE_ALL };

#define QUEUE_LEN 11        /* fetch_queue_length */
#define FETCH_THRESHOLD 8   /* fetch_threshold */
#define PAGE_BITS 7         /* InstBuffer page_bits (words) */
#define REFRESH_DIVIDER 128 /* reset value of the DRAM refresh divider */
#define MAX_OUTSTANDING 4
#define MAX_RECORDS 8

struct espresso_timing
{
  uint16_t (*read_word) (void *ctx, uint32_t addr);
  void *ctx;
  bool memory_map;
  uint64_t cycle;
  /* Loads/stores that passed decode but weren't accepted by the memory
     unit yet (Pipeline's mem_pending, 2 bits); fetch starts no new burst
     while there are any.  */
  int mem_pending;
  espresso_timing_stats stats;

  /* Instructions from the functional simulator not yet bound.  */
  espresso_timing_insn records[MAX_RECORDS];
  int rec_head, rec_count;
  bool started;
  bool last_bound;          /* set by a cycle that bound a record */
  uint64_t bound_cycle;
  bool refill;              /* a branch was taken and no instruction entered execute since */
  bool shadow;              /* a taken branch entered execute: what follows it until
                               do_branch is on the wrong path */

  /* Pipeline: do_branch is registered at the output of execute.  */
  bool do_branch_r;
  uint32_t target_r;        /* byte address */

  /* InstBuffer.  */
  uint32_t ib_addr;         /* fetch_addr, in words */
  bool ib_request;          /* state == request */
  bool ib_page_limit;
  int ib_outstanding;
  int ib_drop;
  int ib_req_len;
  bool ib_oopb_d;
  uint32_t ib_out_addr[MAX_OUTSTANDING];   /* addresses of outstanding requests, oldest first */
  int ib_out_count;

  /* InstQueue (a Fifo plus its own free counter).  */
  uint32_t q_addr[QUEUE_LEN];
  int q_head, q_count;
  int q_free;               /* empty_cnt */

  /* InstAssemble.  */
  int as_state;
  token as_tok;
  int as_need;              /* words still to come */

  /* Decode output registers.  */
  token dec_tok;

  /* RegFile.  */
  bool rf_outstanding_r;
  bool rf_r1v_r, rf_r2v_r, rf_rsvv_r;
  int8_t rf_r1a_r, rf_r2a_r, rf_rsva_r;
  bool rf_rsv_registered_r;
  uint16_t rf_board;
  bool rf_wfw_r, rf_wfw_d, rf_out_buf_full;

  /* ExecuteStage.  */
  bool s1_was_branch, s2_was_branch;
  bool lockout;
  bool s1_buf;              /* stage_1_fsm buffer valid */
  token s1_tok;
  bool s2_buf;              /* stage_2_fsm buffer valid */
  token s2_tok;
  int s2_unit;              /* s2_exec_unit */
  int s2_ldst;
  bool s2_rrv;              /* s2_result_reg_addr_valid */
  int8_t s2_res;            /* output_port.addr */
  bool reg_en2_d;           /* Reg (stage_2_reg_en) */

  /* MemoryStage.  */
  bool mu_active, mu_pending, mu_gap;
  bool mu_is_csr_r, mu_csr_pen, mu_csr_active;
  bool mu_rnw_r, mu_pwrite_r;
  int mu_byte_en_r;
  uint32_t mu_next_addr;    /* words */

  /* BusIf.  */
  int bs_state;
  int bs_arb_r;
  bool bs_rnw;
  int bs_ws, bs_ws_store;
  bool bs_two;
  int bs_refresh_counter;
  bool bs_refresh_req;
  bool bs_mem_pipe[2], bs_fetch_pipe[2];
};

static void
reset (struct espresso_timing *t, uint32_t pc)
{
  uint16_t (*read_word) (void *, uint32_t) = t->read_word;
  void *ctx = t->ctx;
  bool memory_map = t->memory_map;
  memset (t, 0, sizeof (*t));
  t->read_word = read_word;
  t->ctx = ctx;
  t->memory_map = memory_map;
  t->ib_addr = pc >> 1;
  t->ib_req_len = FETCH_THRESHOLD;
  t->q_free = QUEUE_LEN;
  t->as_state = AS_HAVE_0;
  t->s2_unit = U_INVALID;
  t->bs_state = BS_IDLE;
  t->bs_rnw = true;
  t->rf_r1a_r = t->rf_r2a_r = t->rf_rsva_r = 0;
}

struct espresso_timing *
espresso_timing_new (uint16_t (*read_insn_word) (void *ctx, uint32_t addr), void *ctx,
                     bool espresso_memory_map)
{
  struct espresso_timing *t = calloc (1, sizeof (*t));
  if (t == NULL)
    return NULL;
  build_decode_table ();
  t->read_word = read_insn_word;
  t->ctx = ctx;
  t->memory_map = espresso_memory_map;
  reset (t, 0);
  return t;
}

void
espresso_timing_free (struct espresso_timing *t)
{
  free (t);
}

static bool
is_ld_st (int unit)
{
  return unit == U_LD_ST || unit == U_BRANCH_IND;
}

/* Binds the instruction entering execute to the next record.  */
static void
bind (struct espresso_timing *t, token *tok)
{
  const espresso_timing_insn *r = &t->records[t->rec_head];

  tok->bound = false;
  memset (&tok->rec, 0, sizeof (tok->rec));
  if (t->shadow || t->rec_count == 0)
    return;   /* wrong path: flushed when the branch ahead of it resolves */
  if (r->pc != tok->addr)
    {
      /* The model fetched something else than the functional simulator
         executed (a branch to the next instruction, which the functional
         simulator doesn't report as taken, an exception, self-modifying
         code).  Time this instruction as a branch to the right place.  */
      ++t->stats.resyncs;
      tok->rec = *r;
      tok->rec.taken = true;
      tok->rec.next_pc = r->pc;
      tok->bound = true;
      t->shadow = true;
      return;
    }
  tok->rec = *r;
  tok->bound = true;
  if (r->taken)
    t->shadow = true;
  t->rec_head = (t->rec_head + 1) % MAX_RECORDS;
  --t->rec_count;
  t->last_bound = true;
  t->bound_cycle = t->cycle;
}

static void
cycle (struct espresso_timing *t)
{
  /* ---- Inputs: registered outputs of all stages ---- */
  bool do_branch_r = t->do_branch_r;

  /* InstBuffer request.  */
  uint32_t target_w = t->target_r >> 1;
  bool oopb = do_branch_r && ((t->ib_addr & 0x7fffffff) >> PAGE_BITS) != ((target_w & 0x7fffffff) >> PAGE_BITS);
  bool fetch_req_valid = t->ib_request && !t->ib_page_limit && !oopb;
  uint32_t fetch_req_addr = t->ib_addr;

  /* Execute stage 1 output (registered) towards the memory unit.  */
  bool stage_1_valid = t->s1_buf;
  const token *s1 = &t->s1_tok;
  bool s1_is_ld_st = is_ld_st (s1->d.unit);
  bool mem_in_valid = stage_1_valid && s1_is_ld_st;   /* block_mem is 0: no AV or misalignment */
  bool mem_in_is_csr = s1->d.ldst == L_CSR_LOAD || s1->d.ldst == L_CSR_STORE;
  bool mem_in_rnw = s1->d.ldst == L_LOAD || s1->d.ldst == L_CSR_LOAD;
  bool mem_in_len32 = s1->d.mem_len == M_32;
  uint32_t mem_in_addr = s1->rec.has_mem ? s1->rec.mem_addr : 0x08000000;

  /* MemoryStage request valid.  */
  bool csr_select = mem_in_is_csr;
  bool mem_req_valid = ((mem_in_valid && !csr_select) || t->mu_active) && !t->mu_gap;

  /* ---- BusIf: arbitration and ready ---- */
  int arb_comb = t->bs_refresh_req ? PORT_REFRESH : mem_req_valid ? PORT_MEM : PORT_FETCH;
  bool bs_idle = t->bs_state == BS_IDLE;
  int arb = bs_idle ? arb_comb : t->bs_arb_r;
  bool req_ready = bs_idle || t->bs_state == BS_FIRST || t->bs_state == BS_MIDDLE;
  bool bus_mem_ready = req_ready && arb == PORT_MEM;
  bool bus_fetch_ready = req_ready && arb == PORT_FETCH;
  bool bus_mem_rsp_valid = t->bs_mem_pipe[1];
  bool bus_fetch_rsp_valid = t->bs_fetch_pipe[1];

  /* ---- MemoryStage ---- */
  bool mu_in_ready = (bus_mem_ready && !t->mu_active && !t->mu_gap)
                     || (csr_select && mem_in_valid && !t->mu_csr_active);
  bool mu_input_advance = mu_in_ready && mem_in_valid;
  bool mu_bus_req_advance = bus_mem_ready && mem_req_valid;
  bool mu_is_csr = mu_input_advance ? csr_select : t->mu_is_csr_r;
  bool mu_pwrite = mu_input_advance ? !mem_in_rnw : t->mu_pwrite_r;
  bool mu_out_valid = (bus_mem_rsp_valid && !t->mu_pending) || (t->mu_csr_pen && !mu_pwrite);
  bool mu_rnw = mu_input_advance ? mem_in_rnw : t->mu_rnw_r;
  uint32_t mem_first_addr = (mem_in_addr >> 1) & 0x7fffffff;
  uint32_t mem_req_addr = mu_input_advance ? mem_first_addr : t->mu_next_addr;
  int mem_byte_en_in = s1->d.mem_len != M_8 ? 3 : ((mem_in_addr & 1) ? 2 : 1);
  int mem_byte_en = mu_input_advance ? mem_byte_en_in : t->mu_byte_en_r;
  bool mu_next_active = (mem_in_len32 && !mu_is_csr && mu_input_advance) ? true
                        : (mu_bus_req_advance ? false : t->mu_active);

  /* ---- BusIf: request and next state ---- */
  bool req_valid = arb == PORT_FETCH ? fetch_req_valid : arb == PORT_MEM ? mem_req_valid : true;
  uint32_t req_addr = arb == PORT_FETCH ? fetch_req_addr : mem_req_addr;
  bool req_rnw = arb == PORT_FETCH ? true : arb == PORT_MEM ? mu_rnw : true;
  int req_byte_en = arb == PORT_FETCH ? 3 : mem_byte_en;
  bool start = bs_idle && req_valid;
  bool is_nram = t->memory_map && ((req_addr >> 25) & 3) == 0;
  bool req_mem_or_fetch = arb == PORT_MEM || arb == PORT_FETCH;
  bool req_dram = !is_nram && req_mem_or_fetch;
  bool req_nram = is_nram && req_mem_or_fetch;
  bool req_rfsh = arb == PORT_REFRESH;
  int req_ws = ((req_addr >> 27) - 1) & 0xf;
  bool waiting = t->bs_ws != 0;
  bool read_active = (t->bs_state == BS_FIRST || t->bs_state == BS_MIDDLE
                      || (t->bs_state == BS_ND_WAIT && !waiting && !t->bs_two)
                      || (t->bs_state == BS_ND_DUAL_WAIT && !waiting)) && t->bs_rnw;
  bool fetch_req_advance = fetch_req_valid && bus_fetch_ready;

  int bs_next = t->bs_state;
  int ws_dec = t->bs_state == BS_ND_WAIT || t->bs_state == BS_ND_DUAL_WAIT;
  int ws_next;
  bool refresh_tc, refresh_rsp;

  switch (t->bs_state)
    {
    case BS_IDLE:
      if (req_valid && req_nram) bs_next = BS_ND_FIRST;
      else if (req_valid && req_dram) bs_next = BS_FIRST;
      else if (req_valid && req_rfsh) bs_next = BS_REFRESH;
      break;
    case BS_FIRST: bs_next = req_valid ? BS_MIDDLE : BS_PRECHARGE; break;
    case BS_MIDDLE: bs_next = req_valid ? BS_MIDDLE : BS_PRECHARGE; break;
    case BS_PRECHARGE: bs_next = BS_IDLE; break;
    case BS_ND_FIRST: bs_next = BS_ND_WAIT; break;
    case BS_ND_WAIT: bs_next = waiting ? BS_ND_WAIT : (t->bs_two ? BS_ND_DUAL : BS_ND_LAST); break;
    case BS_ND_DUAL: bs_next = BS_ND_DUAL_FIRST; break;
    case BS_ND_DUAL_FIRST: bs_next = BS_ND_DUAL_WAIT; break;
    case BS_ND_DUAL_WAIT: bs_next = waiting ? BS_ND_DUAL_WAIT : BS_ND_LAST; break;
    case BS_ND_LAST: bs_next = BS_IDLE; break;
    case BS_REFRESH: bs_next = BS_IDLE; break;
    }
  ws_next = start ? req_ws
            : (t->bs_ws == 0 ? (t->bs_state == BS_ND_DUAL ? t->bs_ws_store : 0) : t->bs_ws - ws_dec);
  refresh_tc = t->bs_refresh_counter == 0;
  refresh_rsp = t->bs_state == BS_REFRESH;

  /* ---- ExecuteStage, stage 2 ---- */
  {
  const token *s2 = &t->s2_tok;
  bool s2_is_ld_st = is_ld_st (t->s2_unit);
  bool s2_is_load = t->s2_ldst == L_LOAD || t->s2_ldst == L_CSR_LOAD;
  bool out_ready2 = (s2_is_ld_st && s2_is_load) ? mu_out_valid : true;
  bool fsm2_in_ready = !t->s2_buf || out_ready2;
  bool stage_2_ready = s1_is_ld_st ? mu_in_ready : fsm2_in_ready;
  bool stage_2_valid = (s2_is_ld_st ? (mu_out_valid || (t->s2_rrv && t->s2_ldst == L_STORE)) : t->s2_buf)
                       && !t->s2_was_branch;
  bool stage_2_reg_en = stage_1_valid && stage_2_ready && !do_branch_r;
  bool decided_is_s2 = t->s2_unit == U_BRANCH_IND;
  bool sideband_strobe = decided_is_s2 ? mu_out_valid : (s1->d.unit != U_BRANCH_IND && stage_2_reg_en);
  const token *decided = decided_is_s2 ? s2 : s1;
  bool do_branch = sideband_strobe && decided->rec.taken;
  uint32_t branch_target = decided->rec.next_pc;

  /* Write-back towards the register file (ResultExtendStage is combinational).  */
  bool wb_valid = stage_2_valid && t->s2_rrv && (t->reg_en2_d || (mu_out_valid && s2_is_ld_st));
  int8_t wb_addr = t->s2_res;

  /* ---- RegFile response side ---- */
  bool rf_rsp_valid = !t->rf_wfw_d && t->rf_out_buf_full;
  const token *dt = &t->dec_tok;

  /* ---- ExecuteStage, stage 1 ---- */
  bool exec_in_valid = rf_rsp_valid;
  /* indirect_branch_lockout: the next instruction stays out of stage 1 until
     an indirect branch ahead of it is decided.  */
  bool ind_lockout = (t->s1_buf && s1->d.unit == U_BRANCH_IND) || t->s2_unit == U_BRANCH_IND;
  bool fsm1_in_valid = !t->lockout && !ind_lockout && !t->s1_was_branch && !do_branch && exec_in_valid;
  bool fsm1_in_ready = !t->s1_buf || stage_2_ready;
  bool exec_in_ready = !t->lockout && !ind_lockout && !t->s1_was_branch && fsm1_in_ready;
  bool stage_1_reg_en = !t->lockout && !ind_lockout && !t->s1_was_branch && fsm1_in_valid && fsm1_in_ready;

  /* ---- RegFile ---- */
  bool rf_rsp_ready = exec_in_ready;
  bool rf_req_ready = !t->rf_wfw_d && (rf_rsp_ready || !t->rf_out_buf_full);

  /* Decode stage request (from the assembled instruction).  */
  bool as_terminal = t->as_state == AS_HAVE_ALL;
  bool dec_in_valid = as_terminal && !do_branch_r;
  const token *at = &t->as_tok;
  bool rf_req_valid = dec_in_valid && !do_branch_r;
  bool req_r1v = at->d.rd1 >= 0 && !do_branch_r;
  bool req_r2v = at->d.rd2 >= 0 && !do_branch_r;
  bool req_rsvv = at->d.res >= 0 && !do_branch_r;
  int8_t req_r1a = at->d.rd1 >= 0 ? at->d.rd1 : 0;
  int8_t req_r2a = at->d.rd2 >= 0 ? at->d.rd2 : 0;
  int8_t req_rsva = at->d.res >= 0 ? at->d.res : 0;

  bool rf_req_advance = rf_req_ready && rf_req_valid;
  bool rf_rsp_advance = rf_rsp_ready && rf_rsp_valid;
  bool outstanding_req = rf_req_advance || (!rf_rsp_advance && t->rf_outstanding_r);
  bool r1v = rf_req_advance ? req_r1v : t->rf_r1v_r;
  bool r2v = rf_req_advance ? req_r2v : t->rf_r2v_r;
  bool rsvv = rf_req_advance ? req_rsvv : t->rf_rsvv_r;
  int8_t r1a = rf_req_advance ? req_r1a : t->rf_r1a_r;
  int8_t r2a = rf_req_advance ? req_r2a : t->rf_r2a_r;
  int8_t rsva = rf_req_advance ? req_rsva : t->rf_rsva_r;
  bool rsv_registered = !rf_req_advance && t->rf_rsv_registered_r;
#define RSV_BIT(a) ((t->rf_board >> (a)) & 1)
#define WAIT(v, a) (outstanding_req && (v) && RSV_BIT (a) && !(wb_addr == (a) && wb_valid))
  bool wait_r1 = WAIT (r1v, r1a) && (r1a != rsva || !rsv_registered);
  bool wait_r2 = WAIT (r2v, r2a) && (r2a != rsva || !rsv_registered);
  bool wait_rsv_raw = WAIT (rsvv, rsva);
  bool wait_rsv = wait_rsv_raw && !rsv_registered;
#undef WAIT
#undef RSV_BIT
  bool wait_some = wait_r1 || wait_r2 || wait_rsv;
  bool wfw_cond = rf_req_advance || wb_valid;
  bool wait_for_write = wfw_cond ? wait_some : t->rf_wfw_r;
  bool rsv_set_valid = rsvv && !wait_for_write && outstanding_req;
  bool rsv_clr_valid = wb_valid && (!wait_rsv || rsva != wb_addr);

  /* ---- Decode ---- */
  bool register_outputs = rf_req_ready && rf_req_valid;
  bool break_burst = register_outputs && at->d.unit == U_LD_ST;

  /* ---- InstAssemble and InstQueue ---- */
  bool q_valid = t->q_count > 0;
  bool q_full = t->q_count == QUEUE_LEN;
  bool decode_ready = rf_req_ready;
  bool fetch_ready = !as_terminal || decode_ready || do_branch_r;
  bool fsm_advance = (!as_terminal && q_valid && fetch_ready) || (as_terminal && decode_ready);
  bool q_pop = q_valid && fetch_ready;
  uint32_t q_word_addr = q_valid ? t->q_addr[t->q_head] : 0;

  /* ---- InstBuffer ---- */
  bool ib_adv_rsp = bus_fetch_rsp_valid;
  int next_outstanding = t->ib_outstanding + (fetch_req_advance ? 1 : 0) - (ib_adv_rsp ? 1 : 0);
  /* No new burst in the cycle a load/store passes decode (break_burst),
     nor while one is on its way to the memory unit (fetch_hold).  */
  bool start_new_request = ((t->q_free > FETCH_THRESHOLD + next_outstanding) || t->ib_oopb_d)
                           && !break_burst && t->mem_pending == 0;
  bool q_push_valid = ib_adv_rsp && t->ib_drop == 0 && !do_branch_r;
  bool q_push = q_push_valid && !q_full;

  /* ---- Statistics for this cycle ---- */
  ++t->stats.cycles;
  if (fetch_req_advance)
    ++t->stats.fetch_beats;
  if (ib_adv_rsp && !q_push)
    ++t->stats.fetch_dropped;
  if (mu_bus_req_advance)
    ++t->stats.mem_beats;
  if (refresh_rsp)
    ++t->stats.refresh;
  if (stage_1_reg_en)
    ++t->stats.insns;
  else if (t->refill || do_branch || do_branch_r)
    ++t->stats.stall_branch;
  else if (t->lockout)
    ++t->stats.stall_mult;
  else if (exec_in_valid || (t->s1_buf && !stage_2_ready))
    ++t->stats.stall_mem;
  else if (t->rf_wfw_d)
    ++t->stats.stall_dep;
  else
    ++t->stats.stall_fetch;

  /* ======== Clock edge: commit next values ======== */
  {
  /* Records: the instruction entering execute (into stage 1 below).  */
  if (stage_1_reg_en)
    bind (t, &t->dec_tok);

  /* Pipeline.  A branch or exception flushes the pending loads/stores.  */
  if (do_branch_r)
    t->mem_pending = 0;
  else
    {
      bool ldst_issued = mu_input_advance && s1->d.unit == U_LD_ST;
      if (break_burst && !ldst_issued)
        t->mem_pending = (t->mem_pending + 1) & 3;
      else if (ldst_issued && !break_burst && t->mem_pending != 0)
        t->mem_pending--;
    }
  t->do_branch_r = do_branch;
  if (do_branch)
    t->target_r = branch_target;
  if (do_branch)
    {
      t->refill = true;
      t->shadow = false;
    }
  else if (stage_1_reg_en)
    t->refill = false;

  /* BusIf.  */
  t->bs_arb_r = arb;
  if (start)
    {
      t->bs_rnw = req_rnw;
      t->bs_ws_store = req_ws;
      t->bs_two = req_byte_en == 3 && req_nram;
    }
  t->bs_ws = ws_next;
  t->bs_mem_pipe[1] = t->bs_mem_pipe[0];
  t->bs_mem_pipe[0] = read_active && arb == PORT_MEM;
  t->bs_fetch_pipe[1] = t->bs_fetch_pipe[0];
  t->bs_fetch_pipe[0] = read_active && arb == PORT_FETCH;
  t->bs_state = bs_next;
  {
    bool req = t->bs_refresh_req;
    t->bs_refresh_req = (refresh_tc) ? true : (refresh_rsp ? false : req);
    t->bs_refresh_counter = refresh_tc ? REFRESH_DIVIDER : (req ? t->bs_refresh_counter : t->bs_refresh_counter - 1);
  }

  /* MemoryStage.  */
  {
    bool next_pending = (mem_in_len32 && !mu_is_csr && mu_input_advance && mem_in_rnw) ? true
                        : (bus_mem_rsp_valid ? false : t->mu_pending);
    bool next_gap = (!mem_in_len32 && !mu_is_csr && mu_input_advance) || (t->mu_active && !mu_next_active);
    bool next_is_csr = mu_input_advance ? csr_select : (mu_out_valid ? false : t->mu_is_csr_r);
    bool next_csr_pen = mu_input_advance ? mu_is_csr : false;
    bool psel = (mu_input_advance && mu_is_csr) || t->mu_csr_pen;
    if (mu_input_advance)
      {
        t->mu_rnw_r = mem_in_rnw;
        t->mu_pwrite_r = !mem_in_rnw;
        t->mu_byte_en_r = mem_byte_en_in;
        t->mu_next_addr = mem_first_addr | 1;
      }
    t->mu_active = mu_next_active;
    t->mu_pending = next_pending;
    t->mu_gap = next_gap;
    t->mu_is_csr_r = next_is_csr;
    t->mu_csr_pen = next_csr_pen;
    t->mu_csr_active = psel;
  }

  /* ExecuteStage.  */
  {
    bool fsm2_in_valid = stage_1_valid;
    bool s2_buf_next = (fsm2_in_valid && fsm2_in_ready) ? true : ((out_ready2 && t->s2_buf) ? false : t->s2_buf);
    bool s1_buf_next = do_branch ? false
                       : ((fsm1_in_valid && fsm1_in_ready) ? true : ((stage_2_ready && t->s1_buf) ? false : t->s1_buf));
    /* An indirect branch leaves stage 2 once decided (its load returned).  */
    int s2_unit_next = do_branch ? U_INVALID
                       : (stage_2_reg_en ? s1->d.unit
                          : ((t->s2_unit == U_BRANCH_IND && mu_out_valid) ? U_INVALID : t->s2_unit));
    if (stage_2_reg_en)
      {
        t->s2_rrv = s1->d.res >= 0;
        t->s2_res = s1->d.res;
        t->s2_ldst = s1->d.ldst;
        t->s2_tok = *s1;
      }
    t->s2_unit = s2_unit_next;
    t->s2_buf = s2_buf_next;
    t->reg_en2_d = stage_2_reg_en;
    t->lockout = exec_in_ready && exec_in_valid && dt->d.unit == U_MULT;
    t->s2_was_branch = t->s1_was_branch;
    t->s1_was_branch = do_branch;
    if (stage_1_reg_en)
      t->s1_tok = t->dec_tok;
    t->s1_buf = s1_buf_next;
  }

  /* RegFile.  */
  {
    bool rr_cond = rsv_set_valid && !rsv_registered;
    uint16_t board = t->rf_board;

    t->rf_rsv_registered_r = rr_cond ? !wait_rsv_raw : (rf_req_advance ? false : rsv_registered);
    t->rf_outstanding_r = rf_req_advance ? true : (rf_rsp_advance ? false : outstanding_req);
    if (rf_req_advance)
      {
        t->rf_r1a_r = req_r1a;
        t->rf_r2a_r = req_r2a;
        t->rf_rsva_r = req_rsva;
      }
    t->rf_r1v_r = do_branch_r ? false : r1v;
    t->rf_r2v_r = do_branch_r ? false : r2v;
    t->rf_rsvv_r = do_branch_r ? false : rsvv;
    if (do_branch_r)
      board = 0;
    else
      {
        if (rsv_clr_valid && wb_addr >= 0)
          board &= ~(1u << wb_addr);
        if (rsv_set_valid)
          board |= 1u << rsva;
      }
    t->rf_board = board;
    t->rf_wfw_r = do_branch_r ? false : (wfw_cond ? wait_some : wait_for_write);
    t->rf_wfw_d = do_branch_r ? false : wait_for_write;
    t->rf_out_buf_full = do_branch_r ? false
                         : (rf_req_advance ? true : (rf_rsp_advance ? false : t->rf_out_buf_full));
  }

  /* Decode output registers.  */
  if (register_outputs)
    t->dec_tok = *at;

  /* InstAssemble.  */
  if (do_branch_r)
    t->as_state = AS_HAVE_0;
  else if (fsm_advance)
    {
      bool take_word = !as_terminal || q_valid;
      if (as_terminal && !q_valid)
        t->as_state = AS_HAVE_0;
      else if (take_word)
        {
          if (t->as_state == AS_HAVE_0 || t->as_state == AS_HAVE_ALL)
            {
              uint16_t w = t->read_word (t->ctx, q_word_addr << 1);
              token *tok = &t->as_tok;
              tok->addr = q_word_addr << 1;
              tok->word = w;
              tok->words = inst_words (w);
              tok->d = decode_table[w];
              tok->bound = false;
              t->as_need = tok->words - 1;
            }
          else
            --t->as_need;
          t->as_state = t->as_need == 0 ? AS_HAVE_ALL : (t->as_need == 1 ? AS_NEED_1 : AS_NEED_2);
        }
    }

  /* InstQueue.  */
  if (do_branch_r)
    {
      t->q_count = 0;
      t->q_head = 0;
      t->q_free = QUEUE_LEN;
    }
  else
    {
      if (q_pop)
        {
          t->q_head = (t->q_head + 1) % QUEUE_LEN;
          --t->q_count;
        }
      if (q_push)
        {
          t->q_addr[(t->q_head + t->q_count) % QUEUE_LEN] = t->ib_out_count > 0 ? t->ib_out_addr[0] : 0;
          ++t->q_count;
        }
      t->q_free = t->q_free + (q_pop ? 1 : 0) - (q_push ? 1 : 0);
    }

  /* InstBuffer.  */
  {
    int req_len = t->ib_req_len;
    int req_len_next;
    bool page_limit_next = t->ib_request ? ((t->ib_addr & ((1u << PAGE_BITS) - 1)) == (1u << PAGE_BITS) - 1) : false;
    bool state_next = t->ib_request;

    if (ib_adv_rsp && t->ib_out_count > 0)
      {
        memmove (&t->ib_out_addr[0], &t->ib_out_addr[1], (t->ib_out_count - 1) * sizeof (uint32_t));
        --t->ib_out_count;
      }
    if (fetch_req_advance && t->ib_out_count < MAX_OUTSTANDING)
      t->ib_out_addr[t->ib_out_count++] = fetch_req_addr;
    if (start_new_request)
      req_len_next = do_branch_r ? FETCH_THRESHOLD : (t->q_free >= FETCH_THRESHOLD ? FETCH_THRESHOLD : t->q_free);
    else if (break_burst)
      req_len_next = 0;
    else if (fetch_req_advance)
      req_len_next = req_len > 0 ? req_len - 1 : 0;
    else
      req_len_next = req_len;
    if (!t->ib_request && start_new_request)
      state_next = true;
    else if (t->ib_request && (t->ib_page_limit || req_len == 0))
      state_next = false;
    t->ib_drop = do_branch_r ? next_outstanding : (t->ib_drop == 0 ? 0 : t->ib_drop - (ib_adv_rsp ? 1 : 0));
    t->ib_addr = do_branch_r ? target_w : (t->ib_addr + (fetch_req_advance ? 1 : 0)) & 0x7fffffff;
    t->ib_outstanding = next_outstanding;
    t->ib_req_len = req_len_next;
    t->ib_page_limit = page_limit_next;
    t->ib_request = state_next;
    t->ib_oopb_d = oopb;
  }

  ++t->cycle;
  }
  }
}

uint64_t
espresso_timing_step (struct espresso_timing *t, const espresso_timing_insn *insn)
{
  if (!t->started)
    {
      reset (t, insn->pc);
      t->started = true;
    }
  if (t->rec_count == MAX_RECORDS)
    {
      /* Can't happen: every call binds its record before returning.  */
      t->rec_head = (t->rec_head + 1) % MAX_RECORDS;
      --t->rec_count;
    }
  t->records[(t->rec_head + t->rec_count) % MAX_RECORDS] = *insn;
  ++t->rec_count;
  t->last_bound = false;
  /* Run until this record entered execute.  A model that lost the
     instruction stream completely gives up after a while.  */
  for (int guard = 0; !t->last_bound && guard < 100000; ++guard)
    cycle (t);
  if (!t->last_bound)
    {
      ++t->stats.resyncs;
      t->rec_count = 0;
      t->do_branch_r = true;
      t->target_r = insn->next_pc;
    }
  return t->bound_cycle;
}

void
espresso_timing_set_next_pc (struct espresso_timing *t, uint32_t pc, uint32_t next_pc)
{
  /* The instruction just bound is the one in stage 1.  */
  if (t->s1_tok.bound && t->s1_tok.rec.pc == pc)
    {
      t->s1_tok.rec.next_pc = next_pc;
      t->s1_tok.rec.taken = true;
      t->shadow = true;
    }
}

const espresso_timing_stats *
espresso_timing_get_stats (struct espresso_timing *t)
{
  return &t->stats;
}
