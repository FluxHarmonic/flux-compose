#include <libguile.h>

static void
inner_main (void *closure, int argc, char **argv)
{
  scm_c_primitive_load("init.scm");

  scm_shell(argc, argv);
}

int
main (int argc, char **argv)
{
  scm_boot_guile (argc, argv, inner_main, 0);
  return 0; /* never reached, see inner_main */
}
