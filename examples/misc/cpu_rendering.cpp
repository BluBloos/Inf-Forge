// TODO: the point of this example should be to demonstrate some simple CPU rendering path.
// where, we just use the fallback rendering available.

// but when we are using the fallback rendering, it shouldn't be the case that the backbuffer memory
// is ALWAYS allocated for every single application.

// we should have that the fallback rendering (the memory) is requested for. And indeed, remove the CPU_backend.
// because this "backend" could have been requested by anyone (with any backend already enabled).