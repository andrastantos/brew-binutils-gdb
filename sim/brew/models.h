#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  acc_read,
  acc_write,
  acc_exec,
} brew_access_modes;

extern void brew_model_init(SIM_CPU *scpu);
extern void espresso_model_init(SIM_CPU *scpu);
extern void anachron_model_init(SIM_CPU *scpu);

extern void brew_model_setup_sim(SIM_CPU *scpu, SIM_DESC sd);
extern void brew_model_v2p_addr(SIM_CPU *scpu, uint32_t v_addr, brew_access_modes access_mode, uint32_t *p_addr);
extern uint32_t brew_model_read_csr(SIM_CPU *scpu, uint16_t addr);
extern void brew_model_write_csr(SIM_CPU *scpu, uint16_t addr, uint32_t data);
extern void brew_model_handle_exception(SIM_CPU *scpu);
extern void brew_model_handle_interrupt(SIM_CPU *scpu);
extern void brew_model_reset_cpu(SIM_CPU *scpu, bool is_user_mode_sim);

#ifdef __cplusplus
}
#endif
