#pragma once
#include "Engine_Define.h"

NS_BEGIN(Engine)
NS_BEGIN(Helper)

static int32 Random_Int(int32 min, int32 max) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());

    std::uniform_int_distribution<int32> dist(min, max);
    return dist(gen);
}

static Double Random_Double(Double min, Double max) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());

    std::uniform_real_distribution<Double> dist(min, max);
    return dist(gen);
}

// 확률 기반 불리언(true/false) 반환 (예: 30% 확률로 true -> getBool(0.3))
static Bool Random_Bool(Double probability) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());

    std::bernoulli_distribution dist(probability);
    return dist(gen);
}
NS_END
NS_END