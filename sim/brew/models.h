#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern void brew_model_init(SIM_CPU *scpu);
extern void espresso_model_init(SIM_CPU *scpu);
//extern void anachron_model_init(SIM_CPU *scpu);

extern SIM_RC brew_options_init(SIM_DESC sd);

#ifdef __cplusplus
}
#endif
