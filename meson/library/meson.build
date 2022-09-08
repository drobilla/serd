# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

# General definitions for building libraries.
#
# These are essentially workarounds for Meson/Windows/MSVC.  Unfortunately,
# Meson's default_library option doesn't support shared and static builds very
# well.  In particular, it's often necessary to define different symbols for
# static and shared builds of libraries so that symbols can be exported.  To
# work around this, default_library=both isn't supported on Windows.  On other
# platforms with GCC-like compilers, we can support both because symbols can
# safely be exported in the same way (giving them default visibility) in both
# static and shared builds.

default_library = get_option('default_library')
host_system = host_machine.system()

# Abort on Windows with default_library=both
if host_system == 'windows' and default_library == 'both'
  error('default_library=both is not supported on Windows')
endif

# Set library_suffix to the suffix for libraries
if host_system == 'windows' and default_library == 'shared'
  # Meson appends a version to the name only for DLLs, which leads to
  # inconsistent library names, like `mylib-1-1`.  So, provide no suffix to
  # ultimately get the same name as on other platforms, like `mylib-1`.
  library_suffix = ''
else
  library_suffix = '-@0@'.format(meson.project_version().split('.')[0])
endif
