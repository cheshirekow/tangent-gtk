#include <glibmm_generate_extra_defs/generate_extra_defs.h>
#include <iostream>

#include "tangent/gtkutil/panzoomarea.h"

int main(int, char**) {
  std::cout << get_defs(GTK_TYPE_PANZOOM_AREA);

  return 0;
}
