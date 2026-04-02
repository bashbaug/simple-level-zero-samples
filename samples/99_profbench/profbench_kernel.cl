kernel void inc_buffer(global int* dst)
{
    atomic_inc(dst);
}
