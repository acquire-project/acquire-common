#ifndef H_ACQUIRE_PLATFORM_V0
#define H_ACQUIRE_PLATFORM_V0

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min
#undef max

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct thread
    {
        HANDLE inner_;
    };

    struct event
    {
        HANDLE inner_;
    };

    struct lock
    {
        SRWLOCK inner_;
    };

    struct condition_variable
    {
        CONDITION_VARIABLE inner_;
    };

    struct clock
    {
        LARGE_INTEGER ticks_per_second;
        LARGE_INTEGER origin;
    };

    enum AllocatorHint
    {
        AllocatorHint_Default,
        AllocatorHint_LargePage
    };

    struct file
    {
        HANDLE hfile;
        OVERLAPPED overlapped;
    };

    struct lib
    {
        HMODULE inner;
    };

    /// @brief Open the shared library at `absolute_path`.
    /// @param[out] self This library context to initialize.
    /// @param[in]  absolute_path The full path to the library to load.
    /// @return 1 on success, otherwise 0.
    /// @see lib_close()
    int lib_open(struct lib* self, const char* absolute_path);

    /// @brief Open the shared library located at a path relative to the calling
    ///        module.
    /// @param[out] self This library context to initialize.
    /// @param[in]  name A name used to resolve the full path to the library to
    /// load.
    /// @return 1 on success, otherwise 0.
    /// @see lib_close()
    ///
    /// The `name` is transformed to a library path according to the following
    /// steps:
    /// 1. `name` os appended to the absolute path to the module calling this
    /// function.
    /// 2. The file extension corresponding to a shared library on this system
    /// is appended.
    ///
    /// So "name" becomes "path/to/module/name.so" on linux systems.
    int lib_open_by_name(struct lib* self, const char* name);

    /// @brief Close the shared library.
    /// @param[in] self The library context to close.
    /// @see lib_open()
    void lib_close(struct lib* self);

    /// @brief Load a symbol from a library by name.
    /// @param[in] self The open library context.
    /// @param[in] name The name of symbol to look up.
    /// @return non-zero pointer to symbol on success, otherwise 0.
    /// @see lib_open();
    void* lib_load(struct lib* self, const char* name);

    /// @brief Creates a new non-blocking file for writing.
    /// @return 1 on success, otherwise 0
    int file_create(struct file* file,
                    const char* filename,
                    size_t bytes_of_filename);

    void file_close(struct file* file);

    /// @brief Write the memory in `[beg,end)` to `file` starting at `offset`.
    /// @param file Writable file context
    /// @param offset byte offset from the beginning of the file
    /// @param beg Pointer to the first write
    /// @param end Pointer to just past the last byte to write
    /// @return 1 on success, otherwise 0
    int file_write(const struct file* file,
                   uint64_t offset,
                   const uint8_t* beg,
                   const uint8_t* end);

    /// @param filename NULL-terminated path string
    /// @param nbytes length of the filename string in bytes
    /// @return 1 if the file exists, otherwise 0
    int file_exists(const char* filename, size_t nbytes);

    /// @param filename NULL-terminated path string
    /// @param nbytes length of the filename string in bytes
    /// @return 1 if the file is writable, otherwise 0
    int file_is_writable(const char* filename, size_t nbytes);

    void* memory_alloc(size_t capacity_bytes, enum AllocatorHint hint);

    void memory_free(void* address);

    void clock_init(struct clock* clock);

    void clock_shift_ms(struct clock* clock, double ms);

    /// @returns sets the clock origin and returns it, in tics, relative to
    /// an arbitrary origin.
    uint64_t clock_tic(struct clock* clock);

    /// @returns the clock tics relative to the origin.
    int64_t clock_toc(struct clock* clock);

    /// @returns the time in milliseconds relative to the origin.
    double clock_toc_ms(struct clock* clock);

    /// @returns -1,0,or 1 when a new clock sample is prior, equal, or after the
    /// clock origin.
    int8_t clock_cmp_now(struct clock* clock);

    /// @returns -1,0,or 1 when a 'timestamp';  is prior, equal, or after the
    /// clock origin.
    int8_t clock_cmp(struct clock* clock, uint64_t timestamp);

    /// Sleeps till delay_ms after the last clock tic and resets the clock.
    /// If more than delay_ms have passed, does not sleep.
    ///
    /// @param[in] clock May be null.
    /// @param[in] delay_ms Time to sleep in milliseconds.
    void clock_sleep_ms(struct clock* clock, float delay_ms);

    void lock_init(struct lock* self);

    void lock_acquire(struct lock* self);

    int try_lock_acquire(struct lock* self);

    void lock_release(struct lock* self);

    void condition_variable_init(struct condition_variable* self);

    void condition_variable_wait(struct condition_variable* __restrict self,
                                 struct lock* __restrict lock);

    void condition_variable_notify_all(struct condition_variable* self);

    void event_init(struct event* self);

    void event_destroy(struct event* self);

    void event_notify_all(struct event* self);

    void event_wait(struct event* self);

    void thread_init(struct thread* self);

    uint8_t thread_create(struct thread* self, void (*proc)(void*), void* args);

    void thread_join(struct thread* self);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // H_ACQUIRE_PLATFORM_V0
