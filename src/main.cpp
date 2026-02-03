
#include <algorithm>
#include <cstddef>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <new>
#include <random>
#include <unordered_map>
#include <vector>

#include <unistd.h>

namespace hw1 {
    namespace {
        const std::size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);

        size_t *allocate_aligned_buffer(std::size_t alignment, std::size_t buffer_size) {
            return new(std::align_val_t(alignment)) size_t[buffer_size]();
        }

        void shuffle_buffer(std::size_t *buffer, std::size_t bufsize, std::size_t step, std::size_t assoc) {
            std::vector<size_t> indices(assoc);

            for (std::size_t i = 0; i < assoc; ++i) {
                indices[i] = i;
            }

            std::random_device rd;
            std::mt19937_64 rng(rd());

            std::shuffle(indices.begin(), indices.end(), rng);

            for (std::size_t i = 0; i < bufsize; ++i) {
                buffer[indices[(i % assoc)] * step % bufsize] =
                    indices[(i + 1) % assoc] * step % bufsize;
            }
        }

        template<class T>
        T count_median(std::vector<T> &values) {
            std::sort(values.begin(), values.end());

            std::size_t mid = values.size() / 2;

            if (values.size() % 2) {
                return values[mid];
            } else if (!values.empty()) {
                return (values[mid - 1] + values[mid]) / 2;
            }

            return T();
        }

        std::chrono::high_resolution_clock::time_point now() {
            return std::chrono::high_resolution_clock::now();
        }

        std::size_t perform_buffer_chains_loop(const std::size_t *buffer, std::size_t bufsize, std::size_t assoc) {
            std::size_t res = 0;
            std::size_t start = 0;

            for (std::size_t i = 0; i < assoc; ++i) {
                start = buffer[start];
            }

            res = start;
            start = 0;

            for (std::size_t i = 0; i < bufsize; ++i) {
                start = buffer[start];
            }

            return res + start;
        }

        std::size_t run_cache_size_test(std::size_t *buffer, std::size_t bufsize, std::size_t stride, std::size_t assoc, std::size_t tests_count = 5) {
            using nanos = std::chrono::nanoseconds;

            std::vector<long long> durations;
            durations.reserve(tests_count);

            for (std::size_t i = 0; i < tests_count; ++i) {
                std::size_t n = 0;

                const std::size_t repeats = 5;

                shuffle_buffer(buffer, bufsize, stride, assoc);

                for (std::size_t j = 0; j < repeats; ++j) {
                    auto t = now();

                    std::size_t tmp = perform_buffer_chains_loop(buffer, bufsize, assoc);

                    auto dur = now() - t;

                    long long total_access_time = std::chrono::duration_cast<nanos>(dur).count();

                    long time_per_element_ns = (total_access_time + (tmp & 2)) / bufsize;

                    n += time_per_element_ns;
                }

                durations.push_back(n / repeats);
            }

            return count_median(durations);
        }

        std::size_t run_cache_line_size_test(std::size_t *buffer, std::size_t bufsize, std::size_t stride, std::size_t tests_count, std::size_t warmup = 5) {
            constexpr std::size_t WORD_SIZE = sizeof(void *);

            using nanos = std::chrono::nanoseconds;

            std::vector<std::size_t> durations;
            durations.reserve(tests_count);

            const std::size_t count = bufsize / sizeof(buffer[0]);
            const std::size_t step = stride / WORD_SIZE;

            for (std::size_t i = 0; i < tests_count; ++i) {
                const std::size_t repeats = 5;
                long long n = 0;

                for (std::size_t r = 0; r < repeats; ++r) {
                    for (std::size_t j = 0; j < warmup; ++j) {
                        for (std::size_t k = 0; k < count; k += step) {
                            buffer[k] *= k;
                        }
                    }

                    auto t = now();

                    for (std::size_t j = 0; j < count; j += step) {
                        buffer[j] *= j;
                    }

                    auto dur = now() - t;
                    long long loop_time = std::chrono::duration_cast<nanos>(dur).count();

                    n += loop_time;
                }

                durations.push_back(n / repeats);
            }

            return count_median(durations);
        }
    }

    struct cache_size_test_result_t {
        std::size_t cache_size;
        std::size_t cache_associativity;
    };

    cache_size_test_result_t test_cache_size() {
        constexpr std::size_t REPEATS = 10;
        constexpr std::size_t BUFFER_SIZE = 4 * 1024 * 1024;
        constexpr double CACHE_SIZE_THRESHOLD = 1.4;

        using unit_t = std::size_t;

        unit_t *buffer = allocate_aligned_buffer(PAGE_SIZE, BUFFER_SIZE);

        std::unordered_map<std::size_t, std::size_t> cache_size_freq;
        std::unordered_map<std::size_t, std::size_t> cache_assoc_freq;

        for (size_t i = 0; i < REPEATS; ++i) {
            std::cout << "Round " << i + 1 << "/" << REPEATS << " started\n";

            std::vector<std::size_t> cache_size_candidates;

            std::size_t prev_dur = 0;
            std::size_t prev_cache_size = 0;
            std::size_t prev_cache_assoc = 0;

            for (std::size_t stride = 1024; stride < 64 * 1024; stride *= 2) {
                prev_dur = 0;
                prev_cache_size = 0;
                prev_cache_assoc = 0;

                for (std::size_t assoc = 2; assoc <= 32; assoc += 2) {
                    const std::size_t dur = run_cache_size_test(buffer, BUFFER_SIZE / sizeof(unit_t), stride / sizeof(unit_t), assoc);
                    const std::size_t cache_size = assoc * stride;

                    if (prev_dur > 0 && prev_dur * CACHE_SIZE_THRESHOLD < dur) {
                        cache_size_candidates.push_back(prev_cache_size);
                        cache_assoc_freq[prev_cache_assoc]++;
                    }

                    prev_dur = dur;
                    prev_cache_size = cache_size;
                    prev_cache_assoc = assoc;
                }
            }

            std::for_each(cache_size_candidates.begin(), cache_size_candidates.end(), [&cache_size_freq](std::size_t sz){
                cache_size_freq[sz]++;
            });

            std::cout << "Round " << i + 1 << "/" << REPEATS << " finished\n\n";
        }

        using cache_size_best_result_t = std::pair<std::size_t, std::size_t>;

        cache_size_best_result_t cache_size_best_result;

        for (auto&& [cache_size, freq]: cache_size_freq) {
            if (freq > cache_size_best_result.second) {
                cache_size_best_result.first = cache_size;
                cache_size_best_result.second = freq;
            }
        }

        using cache_assoc_best_result_t = std::pair<std::size_t, std::size_t>;

        cache_assoc_best_result_t cache_assoc_best_result;

        for (auto&& [cache_assoc, freq]: cache_assoc_freq) {
            if (freq > cache_assoc_best_result.second) {
                cache_assoc_best_result.first = cache_assoc;
                cache_assoc_best_result.second = freq;
            }
        }

        delete[] buffer;

        return {
            /* .cache_size = */ cache_size_best_result.first,
            /* .cache_associativity = */ cache_assoc_best_result.first
        };
    }

    struct cache_line_test_result_t {
        std::size_t cache_line_size;
    };

    cache_line_test_result_t test_cache_line_size() {
        using cache_line_size_t = std::size_t;
        using time_nanos_t = std::size_t;

        constexpr std::size_t BUFFER_SIZE = 8 * 1024 * 1024;
        constexpr std::size_t WORD_SIZE = sizeof(void *);

        constexpr cache_line_size_t MIN_CACHE_LINE_SIZE = 2 * WORD_SIZE;
        constexpr cache_line_size_t MAX_CACHE_LINE_SIZE = 512;

        constexpr double CACHE_LINE_THRESHOLD = 1.4;

        std::map<cache_line_size_t, time_nanos_t> cache_line_loop_time;

        size_t *buffer = allocate_aligned_buffer(PAGE_SIZE, BUFFER_SIZE);

        for (cache_line_size_t cache_line = MIN_CACHE_LINE_SIZE; cache_line <= MAX_CACHE_LINE_SIZE; cache_line *= 2) {
            cache_line_loop_time[cache_line] = run_cache_line_size_test(buffer, BUFFER_SIZE, cache_line, 5);
        }

        delete[] buffer;

        cache_line_test_result_t test_result{ 0 };

        std::pair<cache_line_size_t, time_nanos_t> prev_result{ 0, 0 };

        for (auto &t: cache_line_loop_time) {
            time_nanos_t prev_time = prev_result.second;
            time_nanos_t time = t.second;

            if (time * CACHE_LINE_THRESHOLD < prev_time && test_result.cache_line_size == 0) {
                test_result.cache_line_size = prev_result.first / 2;

                break;
            }

            prev_result = t;
        }

        return test_result;
    }
}

int main() {
    std::cout << "L1 cache line size determining...\n";
    hw1::cache_line_test_result_t cache_line_test_result = hw1::test_cache_line_size();
    std::cout << "L1 cache line size determined\n\n";

    std::cout << "L1 cache size and associativity determining...\n";
    hw1::cache_size_test_result_t cache_size_test_result = hw1::test_cache_size();
    std::cout << "L1 cache size and associativity determined\n\n";

    std::cout << "L1 cache line size: " << cache_line_test_result.cache_line_size << " B\n";
    std::cout << "L1 cache size: " << cache_size_test_result.cache_size << " B\n";
    std::cout << "L1 cache associativity: " << cache_size_test_result.cache_associativity << "\n";

    return 0;
}
