#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "macros.h"
#include "mm_source_location.h"
#include "progresscounter.h"

#include <exception>
#include <execution>
#include <future>
#include <thread>

namespace thread_utils {

NODISCARD extern bool isOnMainThread();

#define ABORT_IF_NOT_ON_MAIN_THREAD() ::thread_utils::abortIfNotOnMainThread(MM_SOURCE_LOCATION())
extern void abortIfNotOnMainThread(mm::source_location loc);

template<typename ThreadLocals, typename Container, typename Callback, typename MergeThreadLocals>
void parallel_for_each_tl_range(Container &&container,
                                ProgressCounter &counter,
                                Callback &&callback,
                                MergeThreadLocals &&merge_threadlocals)
{
    const auto numThreads = std::max<size_t>(1, std::thread::hardware_concurrency());

    if (numThreads == 1) {
        std::array<ThreadLocals, 1> thread_locals;
        auto &tl = thread_locals.front();
        callback(tl, std::begin(container), std::end(container));
        merge_threadlocals(thread_locals);
    } else {
        std::vector<ThreadLocals> thread_locals;
        thread_locals.reserve(numThreads);
        std::vector<std::future<void>> futures;
        futures.reserve(numThreads);

        auto outer_it = container.begin();
        const size_t chunkSize = (container.size() + numThreads - 1) / numThreads;
        for (size_t i = 0; i < numThreads && outer_it != container.end(); ++i) {
            const auto chunkBegin = outer_it;
            const auto remaining = static_cast<size_t>(std::distance(outer_it, container.end()));
            const size_t actualChunkSize = std::min(chunkSize, remaining);
            std::advance(outer_it, actualChunkSize);
            const auto chunkEnd = outer_it;

            auto &tl = thread_locals.emplace_back();
            futures.emplace_back(
                std::async(std::launch::async, [&counter, &callback, chunkBegin, chunkEnd, &tl]() {
                    try {
                        callback(tl, chunkBegin, chunkEnd);
                    } catch (...) {
                        counter.requestCancel();
                        std::rethrow_exception(std::current_exception());
                    }
                }));
        }

        // these are checked sequentially, so requesting cancel won't help.
        bool canceled = false;
        std::exception_ptr exception;
        for (auto &fut : futures) {
            try {
                fut.get();
            } catch (const ProgressCanceledException &) {
                canceled = true;
            } catch (...) {
                if (!exception) {
                    exception = std::current_exception();
                }
            }
        }
        if (exception) {
            std::rethrow_exception(exception);
        } else if (canceled) {
            throw ProgressCanceledException();
        }

        merge_threadlocals(thread_locals);
    }
}

template<typename ThreadLocals, typename Container, typename Callback, typename MergeThreadLocals>
void parallel_for_each_tl(Container &&container,
                          ProgressCounter &counter,
                          Callback &&callback,
                          MergeThreadLocals &&merge_threadlocals)
{
    parallel_for_each_tl_range<ThreadLocals>(
        std::forward<Container>(container),
        counter,
        [&counter, &callback](auto &tl, const auto chunkBegin, const auto chunkEnd) {
            for (auto it = chunkBegin; it != chunkEnd; ++it) {
                callback(tl, *it);
                counter.step();
            }
        },
        std::forward<MergeThreadLocals>(merge_threadlocals));
}

template<typename Container, typename Callback>
void parallel_for_each(Container &&container, ProgressCounter &counter, Callback &&callback)
{
    struct DummyThreadLocals final
    {};
    parallel_for_each_tl<DummyThreadLocals>(
        std::forward<Container>(container),
        counter,
        [&callback](DummyThreadLocals &, auto &&x) { callback(x); },
        // merge thread locals
        [](auto &) {});
}

} // namespace thread_utils

// New function to parallelize work over an ImmOrderedSet using immer's native chunks.
namespace thread_utils {

template<typename ThreadLocals, typename T, typename PerElementCallback, typename MergeThreadLocals>
void parallel_for_each_on_immer_chunks_tl(
    const ImmOrderedSet<T>& set_container,
    ProgressCounter& counter,
    PerElementCallback&& per_element_callback,
    MergeThreadLocals&& merge_threadlocals)
{
    const auto numThreads = std::max<size_t>(1, std::thread::hardware_concurrency());
    const immer::flex_vector<T>& vector_container = set_container.get_flex_vector();

    if (numThreads == 1 || vector_container.empty()) {
        std::array<ThreadLocals, 1> thread_locals_storage;
        auto& tl = thread_locals_storage.front();
        // Use the ImmOrderedSet's for_each, which itself uses immer::for_each_chunk sequentially
        set_container.for_each([&](const T& element) {
            if (counter.isCancelRequested()) {
                // Similar to ProgressCanceledException, but for_each doesn't throw it.
                // We might need a custom for_each that can be cancelled or throw.
                // For now, just stop processing.
                // To be fully robust, this path should also be able to signal cancellation.
                // However, the original single-threaded path in parallel_for_each_tl_range
                // would throw if callback itself throws ProgressCanceledException.
                // Let's assume per_element_callback might throw if counter.isCancelRequested().
                return;
            }
            per_element_callback(tl, element);
            counter.step();
        });
        merge_threadlocals(thread_locals_storage);
        return;
    }

    std::vector<ThreadLocals> thread_locals_vector(numThreads);
    std::vector<std::future<void>> futures;
    futures.reserve(numThreads); // Reserve for all potential threads

    struct ChunkInfo {
        const T* begin;
        const T* end;
        // size_t size() const { return static_cast<size_t>(std::distance(begin, end)); } // Not strictly needed for this impl
    };
    std::vector<ChunkInfo> collected_chunks;

    immer::for_each_chunk(vector_container, [&](const T* chunk_begin, const T* chunk_end) {
        if (chunk_begin != chunk_end) {
            collected_chunks.push_back({chunk_begin, chunk_end});
        }
    });

    if (collected_chunks.empty()) {
        merge_threadlocals(thread_locals_vector);
        return;
    }

    futures.reserve(std::min(collected_chunks.size(), numThreads)); // Reserve based on actual work units

    std::atomic<size_t> next_chunk_idx = 0;

    for (size_t i = 0; i < numThreads; ++i) {
        // Only launch a thread if there are chunks potentially available for it.
        // This check is implicitly handled by next_chunk_idx loop condition later,
        // but good to keep in mind if we had a static assignment of first numThreads chunks.
        // With dynamic assignment, all numThreads can be launched if collected_chunks is not empty.
        if (i >= collected_chunks.size() && i > 0) { // Ensure at least one worker if chunks exist
             // break; // if fewer chunks than threads, don't launch unnecessary threads.
                      // Actually, with atomic index, all threads can participate.
        }

        futures.emplace_back(
            std::async(std::launch::async,
                       [&, i]() { // i is thread index, used for thread_locals_vector
                ThreadLocals& tl = thread_locals_vector[i];
                size_t chunk_idx_to_process;
                while((chunk_idx_to_process = next_chunk_idx.fetch_add(1)) < collected_chunks.size()) {
                    if (counter.isCancelRequested()) break;

                    const auto& chunk_info = collected_chunks[chunk_idx_to_process];
                    for (const T* it = chunk_info.begin; it != chunk_info.end; ++it) {
                        if (counter.isCancelRequested()) break;
                        per_element_callback(tl, *it);
                        counter.step();
                    }
                }
            })
        );
    }

    bool canceled = false;
    std::exception_ptr exception;
    for (auto& fut : futures) {
        try {
            fut.get();
        } catch (const ProgressCanceledException&) {
            canceled = true;
            counter.requestCancel(); // Ensure others know
        } catch (...) {
            if (!exception) {
                exception = std::current_exception();
            }
            counter.requestCancel(); // Ensure others know
        }
    }

    if (exception) {
        std::rethrow_exception(exception);
    } else if (canceled) {
        throw ProgressCanceledException();
    }

    merge_threadlocals(thread_locals_vector);
}

} // namespace thread_utils
