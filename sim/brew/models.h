#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern void brew_model_init(SIM_CPU *scpu);
extern void espresso_model_init(SIM_CPU *scpu);
//extern void anachron_model_init(SIM_CPU *scpu);

extern SIM_RC brew_options_init(SIM_DESC sd);

// For the Espresso cycle model (espresso-timing.c): reads a 16-bit instruction
// word through the model's address translation, without exceptions or traces
// (0 if nothing is mapped there); and whether the model has Espresso's memory map.
extern uint16_t brew_timing_read_insn_word(void *scpu, uint32_t vaddr);
extern bool brew_model_has_espresso_map(SIM_CPU *scpu);

#ifdef __cplusplus
}
#endif
