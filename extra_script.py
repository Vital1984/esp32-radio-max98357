Import("env")
import os

include_dir = os.path.join(env.subst("$PROJECT_DIR"), "include")

env.Append(
    CPPPATH=[include_dir],
    CCFLAGS=["-include", "dsps_sf32_compat.h"]
)
