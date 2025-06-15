#ifndef FILTER_H
#define FILTER_H
#include <stddef.h>

// Simple moving average filter over the last ``N`` values.
// ``T`` must support addition and division by ``size_t``.
template<typename T, size_t N>
class SMAFilter {
public:
    // Create a new filter with all entries initialised to zero.
    SMAFilter() : count(0), index(0), sum(0) {
        for(size_t i=0;i<N;i++) values[i]=0;
    }
    // Append a new sample to the window.
    void add(T v) {
        if(count < N) {
            values[index] = v;
            sum += v;
            index = (index + 1) % N;
            count++;
        } else {
            sum -= values[index];
            values[index] = v;
            sum += v;
            index = (index + 1) % N;
        }
    }
    // Return the current average of all stored samples.  Returns zero when
    // no values have been added yet.
    T average() const {
        if(count == 0) return 0;
        return sum / count;
    }
private:
    T values[N];
    size_t count;
    size_t index;
    T sum;
};

#endif // FILTER_H
