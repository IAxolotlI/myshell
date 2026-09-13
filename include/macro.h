#ifndef MACROS_H_
#define MACROS_H_

#ifdef STB_Q_IMPLEMENTATION
#ifndef Q_INIT_CAP
#define Q_INIT_CAP 128
#endif

#define q_empty(q) ((q)->count == 0)

#define q_push(q, item)                                                              \
    do {                                                                             \
        if ((q)->count >= (q)->capacity) {                                           \
            size_t old_cap = (q)->capacity;                                          \
            (q)->capacity = old_cap == 0 ? Q_INIT_CAP : old_cap * 2;                 \
            (q)->items = realloc((q)->items, (q)->capacity * sizeof(*(q)->items));   \
            assert((q)->items != NULL && "Out of RAM");                              \
                                                                                     \
            if ((q)->tail <= (q)->head && old_cap > 0) {                             \
                for (size_t i = 0; i < (q)->tail; ++i) {                             \
                    (q)->items[old_cap + i] = (q)->items[i];                         \
                }                                                                    \
                (q)->tail += old_cap;                                                \
            }                                                                        \
        }                                                                            \
        (q)->items[(q)->tail] = (item);                                              \
        (q)->tail = ((q)->tail + 1) % (q)->capacity;                                 \
        (q)->count++;                                                                \
    } while (0)

#define q_pop(q, item)                                                               \
    do {                                                                             \
        if ((q)->count > 0) {                                                        \
            (item) = (q)->items[(q)->head];                                          \
            (q)->head = ((q)->head + 1) % (q)->capacity;                             \
            (q)->count--;                                                            \
        }                                                                            \
    } while (0)

#define q_free(q)         \
    do {                  \
        free((q)->items); \
    } while (0)
#endif //implementation


#ifdef STB_DA_IMPLEMENTATION
#ifndef DA_INIT_CAP
#define DA_INIT_CAP 128
#endif

#define da_append(da, item)                                                          \
    do {                                                                             \
        if ((da)->count >= (da)->capacity) {                                         \
            (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Out of RAM");                             \
        }                                                                            \
                                                                                     \
        (da)->items[(da)->count++] = (item);                                         \
    } while (0)

#define da_remove(da, index)                        \
    do {                                            \
        size_t j = (index);                         \
        assert(j < (da)->count);                    \
        for (size_t i = j; i < (da)->count - 1; i++) { \
            (da)->items[i] = (da)->items[i + 1];    \
        }                                           \
        (da)->count--;                              \
    } while (0)

    
#define da_append_state(da, item)                                                                       \
    do {                                                                                                \
        if ((da)->count_states >= (da)->capacity_states) {                                              \
            (da)->capacity_states = (da)->capacity_states == 0 ? DA_INIT_CAP : (da)->capacity_states*2; \
            (da)->states = realloc((da)->states, (da)->capacity_states*sizeof(*(da)->states));          \
            assert((da)->states != NULL && "Out of RAM");                                               \
        }                                                                                               \
                                                                                                        \
        (da)->states[(da)->count_states++] = (item);                                                    \
    } while (0)

#endif // implementation
#endif // MACROS_H_
