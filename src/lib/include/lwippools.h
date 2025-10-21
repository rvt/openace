/* OPTIONAL: Pools to replace heap allocation
 * Optional: Pools can be used instead of the heap for mem_malloc. If
 * so, these should be defined here, in increasing order according to
 * the pool element size.
 *
 * LWIP_MALLOC_MEMPOOL(number_elements, element_size)
 * 
 * NOTE: When the pools are getting full, then they elsoe interfere with the website, and possible with UDP connections
 * Example: Pool MALLOC_768           | avail:  12 | used:  12 | max:  12 | err: 362
 * In this case the pool is incorrectly sized, this might also happen with TCP issues!
 */

#if MEM_USE_POOLS
    #if defined(PICO_RP2350)
        LWIP_MALLOC_MEMPOOL_START
        LWIP_MALLOC_MEMPOOL(8, 256)
        LWIP_MALLOC_MEMPOOL(6, 512)
        LWIP_MALLOC_MEMPOOL(16, 768)
        LWIP_MALLOC_MEMPOOL(2, 2048)
        LWIP_MALLOC_MEMPOOL_END
    #else
        LWIP_MALLOC_MEMPOOL_START
        LWIP_MALLOC_MEMPOOL(8, 256)
        LWIP_MALLOC_MEMPOOL(8, 512)
        LWIP_MALLOC_MEMPOOL(12, 768)
        LWIP_MALLOC_MEMPOOL(2, 2048)        
        LWIP_MALLOC_MEMPOOL_END
#endif
#endif /* MEM_USE_POOLS */
