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


}

struct brew_model_info {
public:
  brew_model_info() {
    printf("Setting model to BREW\n");
  }
};

struct espresso_model_info: public brew_model_info {
public:
  espresso_model_info() {
    printf("Setting model to ESPRESSO\n");
  }
};

struct anachron_model_info: public espresso_model_info {
public:
  anachron_model_info() {
    printf("Setting model to ANACHRON\n");
  }
};

static void free_model_info(SIM_CPU *scpu)
{
  free(scpu->model_info);
}

void brew_model_init(SIM_CPU *scpu)
{
  scpu->model_info = new brew_model_info();
  scpu->free_model_info = free_model_info;
}

void espresso_model_init(SIM_CPU *scpu)
{
  scpu->model_info = new espresso_model_info();
  scpu->free_model_info = free_model_info;
}

void anachron_model_init(SIM_CPU *scpu)
{
  scpu->model_info = new anachron_model_info();
  scpu->free_model_info = free_model_info;
}
