#ifndef FILTER_H
#define FILTER_H
#include <stddef.h>

// Simple moving average filter
template<typename T, size_t N>
class SMAFilter {
public:
    SMAFilter() : count(0), index(0), sum(0) {
        for(size_t i=0;i<N;i++) values[i]=0;
    }
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
