// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

extern uint ticks;

#define NBUCKETS 13

struct
{
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

struct spinlock bucketlocks[NBUCKETS];

struct buf *hashtable[NBUCKETS][FSSIZE / NBUCKETS + 1];

void binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKETS; i++)
    initlock(&bucketlocks[i], "bacache.bucket");

  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->timestamp = 0;
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucketno = blockno % NBUCKETS;
  int key = blockno / NBUCKETS;

  acquire(&bucketlocks[bucketno]);
  struct buf *cached_buf = hashtable[bucketno][key];
  // Is the block already cached?
  if (cached_buf)
  {
    if (cached_buf->dev == dev)
    {
      cached_buf->refcnt++;
      cached_buf->timestamp = ticks;
      release(&bucketlocks[bucketno]);
      acquiresleep(&cached_buf->lock);
      return cached_buf;
    }
  }

  struct buf *lru_b = 0;
  uint min_timestamp = __UINT16_MAX__;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    if (b->refcnt == 0 && b->timestamp <= min_timestamp)
    {
      min_timestamp = b->timestamp;
      lru_b = b;
    }
  }

  if (!lru_b)
    panic("bget: no buffers");

  uint old_blockno = lru_b->blockno;
  uint old_bucketno = old_blockno % NBUCKETS;
  lru_b->dev = dev;
  lru_b->blockno = blockno;
  lru_b->valid = 0;
  lru_b->refcnt = 1;
  lru_b->timestamp = ticks;
  hashtable[bucketno][key] = lru_b;
  if (old_blockno)
  {
    if (old_bucketno != bucketno)
      acquire(&bucketlocks[old_bucketno]);
    hashtable[old_bucketno][old_blockno / NBUCKETS] = 0;
  }
  if (old_blockno && old_bucketno != bucketno)
    release(&bucketlocks[old_bucketno]);
  release(&bucketlocks[bucketno]);
  acquiresleep(&lru_b->lock);
  return lru_b;
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
}

void bpin(struct buf *b)
{
  uint bucketno = b->blockno % NBUCKETS;
  acquire(&bucketlocks[bucketno]);
  b->refcnt++;
  release(&bucketlocks[bucketno]);
}

void bunpin(struct buf *b)
{
  uint bucketno = b->blockno % NBUCKETS;
  acquire(&bucketlocks[bucketno]);
  b->refcnt--;
  release(&bucketlocks[bucketno]);
}
