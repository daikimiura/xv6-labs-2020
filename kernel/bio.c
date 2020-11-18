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
  struct buf head;
} bcache;

struct spinlock bucketlocks[NBUCKETS];

struct buf *hashtable[NBUCKETS][FSSIZE / NBUCKETS + 1];

void binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    b->timestamp = 0;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }

  for (int i = 0; i < NBUCKETS; i++)
    initlock(&bucketlocks[i], "bacache.bucket");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);

  int bucketno = blockno % NBUCKETS;
  int key = blockno / NBUCKETS;

  acquire(&bucketlocks[bucketno]);
  struct buf *cached_buf = hashtable[bucketno][key];
  // Is the block already cached?
  if (cached_buf)
  {
    // printf("cache hit\n");
    // for (b = bcache.head.next; b != &bcache.head; b = b->next)
    // {
    //   if (b->dev == dev && b->blockno == blockno)
    //   {
    // printf("bcache hit!\n");
    cached_buf->refcnt++;
    cached_buf->timestamp = ticks;
    release(&bucketlocks[bucketno]);
    // release(&bcache.lock);
    acquiresleep(&cached_buf->lock);
    return cached_buf;
    // }
  }

  // printf("not cached\n");
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf *lru_b = bcache.buf;
  uint min_timestamp = lru_b->timestamp;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    if (b->refcnt == 0 && b->timestamp < min_timestamp)
    {
      min_timestamp = b->timestamp;
      lru_b = b;
    }
  }
  uint old_blockno = lru_b->blockno;
  uint old_bucketno = old_blockno % NBUCKETS;
  lru_b->dev = dev;
  lru_b->blockno = blockno;
  lru_b->valid = 0;
  lru_b->refcnt = 1;
  lru_b->timestamp = ticks;
  if (old_blockno)
  {
    if (old_bucketno != bucketno)
      acquire(&bucketlocks[old_bucketno]);
    hashtable[old_bucketno][old_blockno / NBUCKETS] = 0;
  }
  hashtable[bucketno][key] = lru_b;
  if (old_blockno && old_bucketno != bucketno)
    release(&bucketlocks[old_bucketno]);
  release(&bucketlocks[bucketno]);
  // release(&bcache.lock);
  acquiresleep(&lru_b->lock);
  return lru_b;
  //   for (b = bcache.head.prev; b != &bcache.head; b = b->prev)
  //   {
  //   if (b->refcnt == 0)
  //   {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     b->timestamp = ticks;
  //     hashtable[bucketno][key] = b;
  //     release(&bucketlocks[blockno]);
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  panic("bget: no buffers");
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

  // acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->timestamp = 0;
    // // hashtable[b->blockno % NBUCKETS][b->blockno / NBUCKETS] = 0;
    // // b->valid = 0;
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
  }

  // release(&bcache.lock);
}

void bpin(struct buf *b)
{
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void bunpin(struct buf *b)
{
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
