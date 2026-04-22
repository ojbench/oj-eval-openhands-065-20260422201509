#pragma once
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <cstdint>

// External APIs provided by the judge. Do not implement here.
int* getNewBlock(int n);
void freeBlock(const int* block, int n);

class Allocator {
public:
    Allocator() = default;
    ~Allocator() {
        for (auto &blk : blocks_) {
            if (blk.base) {
                freeBlock(blk.base, blk.blocks);
            }
        }
        blocks_.clear();
        allocs_.clear();
        ptr_index_.clear();
    }

    int* allocate(int n) {
        if (n <= 0) return nullptr;
        const int ints_per_block = static_cast<int>(4096 / sizeof(int));
        // Prefer using the last block if it has enough space
        if (!blocks_.empty() && (blocks_.back().capacity - blocks_.back().offset) >= n) {
            Block &b = blocks_.back();
            int start = b.offset;
            int* ptr = b.base + start;
            b.offset += n;
            b.live_allocs += 1;
            record_allocation(ptr, static_cast<int>(blocks_.size() - 1), start, n);
            return ptr;
        }
        // Try to reuse any completely empty block that is large enough
        for (int i = 0; i < static_cast<int>(blocks_.size()); ++i) {
            Block &b = blocks_[i];
            if (i != static_cast<int>(blocks_.size() - 1) && b.live_allocs == 0 && b.capacity >= n) {
                b.offset = 0;
                int start = 0;
                int* ptr = b.base + start;
                b.offset = n;
                b.live_allocs = 1;
                record_allocation(ptr, i, start, n);
                return ptr;
            }
        }
        // Otherwise, acquire a new block sized to fit n
        int needed_blocks = (n + ints_per_block - 1) / ints_per_block;
        if (needed_blocks <= 0) needed_blocks = 1;
        int* base = getNewBlock(needed_blocks);
        Block nb{base, needed_blocks, needed_blocks * ints_per_block, 0, 0};
        blocks_.push_back(nb);
        Block &b = blocks_.back();
        int start = b.offset;
        int* ptr = b.base + start;
        b.offset += n;
        b.live_allocs += 1;
        record_allocation(ptr, static_cast<int>(blocks_.size() - 1), start, n);
        return ptr;
    }

    void deallocate(int* pointer, int /*n*/) {
        if (!pointer) return;
        auto it = ptr_index_.find(pointer);
        if (it == ptr_index_.end()) return; // undefined behavior per spec; ignore
        int idx = it->second;
        Allocation &rec = allocs_[idx];
        if (rec.freed) return;
        rec.freed = true;
        Block &blk = blocks_[rec.block_idx];
        if (blk.live_allocs > 0) blk.live_allocs -= 1;

        // If this was the most recent allocation(s), roll back bump pointer and free empty tail blocks
        while (!allocs_.empty() && allocs_.back().freed) {
            Allocation last = allocs_.back();
            Block &lb = blocks_[last.block_idx];
            if (last.start <= lb.offset) {
                lb.offset = last.start;
            }
            ptr_index_.erase(last.ptr);
            allocs_.pop_back();

            // If the last block becomes entirely empty and it's the tail block, release it
            while (!blocks_.empty()) {
                Block &tb = blocks_.back();
                if (tb.live_allocs == 0 && tb.offset == 0) {
                    freeBlock(tb.base, tb.blocks);
                    blocks_.pop_back();
                } else {
                    break;
                }
            }
        }
    }

private:
    struct Block {
        int* base;
        int blocks;     // number of 4096-byte units
        int capacity;   // in ints
        int offset;     // next free index (in ints)
        int live_allocs;
    };
    struct Allocation {
        int* ptr;
        int block_idx;
        int start; // in ints
        int len;   // in ints
        bool freed;
    };

    void record_allocation(int* ptr, int block_idx, int start, int len) {
        Allocation rec{ptr, block_idx, start, len, false};
        allocs_.push_back(rec);
        ptr_index_[ptr] = static_cast<int>(allocs_.size() - 1);
    }

    std::vector<Block> blocks_;
    std::vector<Allocation> allocs_;
    std::unordered_map<int*, int> ptr_index_;
};
