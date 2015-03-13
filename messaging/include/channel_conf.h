#pragma once

/* Allocate messages out of a separate, private buffer to avoid
 * ping-ponging cache lines with receiver while messages are being
 * constructed. */
/* #define CHANNEL_USE_SCRATCH */

/* Use SSE instructions to copy cache lines. */
/* #define CHANNEL_USE_SSE */

/* Write into cache lines before reading them if they will be written
 * shortly after. */
/* #define CHANNEL_PULL_INTO_CACHE_WRITE */

/* Use atomic instructions to pull into cache */
#define CHANNEL_PULL_INTO_CACHE_CMPXCHG

/* Prefetch the next message into the sender's cache. Seems to degrade
 * performance and shouldn't be used, probably. */
/* #define CHANNEL_PREFETCH */

/* Perform a memory store fence after posting messages. Gives mild to severe
 * performance degradation. */
#define CHANNEL_SFENCE_SEND
#define CHANNEL_SFENCE_FREE

/* This option minimizes cache misses by re-using the buffer to send
 * messages.
 *
 * It breaks the implementation, removing support for multiple
 * outstanding messages (asynchronous messaging or multiple senders).
 *
 * It is a limit study to show how we can reach 2 misses/message
 * avoiding allocation/GC and reusing the same cache line.
 */
/* #define CHANNEL_NO_ALLOCATOR */
