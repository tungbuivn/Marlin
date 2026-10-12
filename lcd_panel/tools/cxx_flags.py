# Add the C++-only flags to CXXFLAGS only, so compiling the core's .c files does not
# produce "valid for C++/ObjC++ but not for C" warnings.
Import("env")

env.Append(CXXFLAGS=["-fno-rtti", "-fno-threadsafe-statics"])
