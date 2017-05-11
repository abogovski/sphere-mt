#ifndef ALGO_INCLUDED
#define ALGO_INCLUDED
#include "file.h"
#include <algorithm>
#include <memory>
#include <iostream>


template <typename T, class LESS>
class MultiFileHeap
{
    struct HeapEntry
    {
		size_t idx;
		T value;
    };
	HeapEntry* heap;
    
    size_t ways;
    size_t active_ways;

	ull blk_size;
    bool last_blk;
	
    std::vector<BarrieredFileReader<T>> src;

    LESS cmp;
    std::function<bool(const HeapEntry&, const HeapEntry&)> revcmp =
		[this](const HeapEntry& a, const HeapEntry& b)->
				bool{ return cmp(b.value, a.value);};

	bool make_required;

public:
    MultiFileHeap(std::vector<File<T>>& files,
                  ull blk_size,
                  size_t ways,
                  T* buf,
                  size_t buf_size,
                  LESS cmp = std::less<T>()):
		heap(new HeapEntry[ways]),
		ways(ways),
        active_ways(0),
        blk_size(blk_size),
        last_blk(false),
        cmp(cmp),
        make_required(true)
    {
		assert(buf_size >= files.size());

		T* first = nullptr;
		T* last = buf;
		
		for (size_t i = 0; i < files.size(); ++i) {
			first = buf + (i * buf_size) / files.size();
			last = buf + ((i + 1) * buf_size) / files.size();
			src.emplace_back(BarrieredFileReader<T>(files[i], blk_size, first, last));
		}
	}

	~MultiFileHeap()
	{
		delete[] heap;
	}
		
    bool pop(T* value)
    {
        make_required = !active_ways;
        if (make_required) {
            return false;
        }

		HeapEntry& back = heap[active_ways-1]; 

		std::pop_heap(heap, heap + active_ways, revcmp);
		*value = back.value;

		if (src[back.idx].get(value)) {
			std::push_heap(heap, heap + active_ways, revcmp);
		} else {
			assert(src[back.idx].eof() || !src[back.idx].eof() && src[back.idx].barrier());

			last_blk = last_blk || src[back.idx].eof();			
			back.value = 0;
			--active_ways;
		}

        return true;
    }

    bool make()
    {
        if (!make_required) {
            throw std::runtime_error("unexpected make");
        }

        if (last_blk) {
            return false;
        }

        active_ways = ways;
        for (size_t i = 0; i < active_ways; ++i) {
            heap[i].idx = i;
			heap[i].value = 0;
        }

        for (size_t i = 0; i < active_ways; ) {
			HeapEntry& cur = heap[i];
			if (src[cur.idx].barrier()) {
				src[cur.idx].proceed();
			}
			
			if (src[cur.idx].get(&cur.value)) {
				++i;
			} else {
				cur = heap[--active_ways];	
			}
        }

        if (!active_ways) {
            return false;
        }

        std::make_heap(heap, heap + active_ways, revcmp);
        make_required = false;
        return true;
    }
};


template <typename T, class LESS = std::less<T>>
void extsort(const char* fname_in, const char* fname_out,
             size_t buf_len, size_t ways, LESS cmp = LESS())
{
    if (buf_len % (2 * ways)) {
        throw std::runtime_error("buf_size % ways should be 0 (" +
                                 std::to_string(buf_len) + "%" + 
                                 std::to_string(ways) + "=" + 
                                 std::to_string(buf_len % ways) + ")");
    }
	std::vector<File<T>> dst = TempFiles<T>(ways);
	std::vector<File<T>> src;

	T* buf = new T[buf_len];
    std::unique_ptr<T> buf_ptr(buf);

	/* sort blocks & distribute them per #ways files */ 
	ull c = 0; // blk_cnt on current pass
	File<T> file_in(fname_in, "rb", true);
	size_t rdcnt = 0;
	do {
        rdcnt = file_in.read(buf, buf_len);
		if (rdcnt > 0) {
            std::sort(buf, buf + buf_len, cmp);
            dst[c++ % ways].write(buf, rdcnt);
        }
    } while (file_in.eof());
	file_in.close();

	/* if sizeof(input) <= sizeof(buffer) */
	if (c <= 1) {
		std::cout << "Nothing to merge" << std::endl;
		File<T> file_out(fname_out, "wb");
		file_out.write(buf, (buf_len + rdcnt) % buf_len);
		return;
	}

	/* merge blocks */
    ull blk_len = buf_len;
    for (T value; c > 1; blk_len *= ways) {
        std::cout << "Merging " <<  c << " blocks" << std::endl;

		/* init next pass */
		src = std::move(dst);
		for (auto it = src.begin(); it != src.end(); ++it) {
			it->rewind();
		}
        if (c > ways) {
            dst = TempFiles<T>(ways);
        } else {
			dst.clear(); // redundant
			dst.emplace_back(File<T>(fname_out, "wb+"));
        }

        c = 0;
        MultiFileHeap<T, LESS> mfh(src, blk_len, ways, buf, buf_len/2, cmp);
		std::vector<FileWriter<T>> dstfw;
		for (size_t i = 0; i < dst.size(); ++i) {
			T* first = buf + buf_len/2 + (i * (buf_len - buf_len/2)) / dst.size();
			T* last = buf + buf_len/2 + ((i+1) * (buf_len - buf_len/2)) / dst.size();
			dstfw.emplace_back(FileWriter<T>(dst[i], first, last));	
		}
        for (size_t i = 0; mfh.make(); i = (i + 1) % ways, ++c) {
            while (mfh.pop(&value)) {
                dstfw[i].put(value);
            }
        }
    }
}

#endif //ALGO_INCLUDED
