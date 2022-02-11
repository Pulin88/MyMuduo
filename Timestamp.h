#pragma once

#include <iostream>
using namespace std;

class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondSinceEpoch);
    static Timestamp now();
    string toString() const;

private:
    int64_t microSecondSinceEpoch_;
};