=========================
Generating gtkmm bindings
=========================

1. Use `apt-get source glibmm-2.4` to get the glib soruces

2. Use tools/defs_gen/h2def.py to generate _methods.defs.

   .. note:: The script requires python2 (at least at version 2.56.0)

   Example::

     python2 ./tools/defs_gen/h2def.py \
       ~/Codes/tangetnsky/tangent/gtkutil/gappstate.h

3. Use tools/enum.py to generate _enums.defs (we have none)

4. Use a custom program to generate _signals.defs and _properties.defs. See
   gtkutil/gen_extra_defs.cc

4. Manually edit _vfuncs.def

5. Create .defs just including the above four files

6. Run ::

     gmmproc <basename> <srcdir> <tgtdir>

   which can be found in `/usr/lib/x86_64-linux-gnu/glibmm-2.4/proc`
