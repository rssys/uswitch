#ifndef MY_HELPERS_H
#define MY_HELPERS_H

#include <stdio.h>
#include <vector>
#include <algorithm>
using namespace std;

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#define ERROR(...) { printf("\nError: "); printf(__VA_ARGS__); exit(1); }

// the remainder of this document are basically maps and folds on vectors

template<class returnType, class argType>
vector<returnType> mapVec(returnType (*fn)(argType), const vector<argType>* v) {
  vector<returnType> returnMe;
  for(argType a : *v) returnMe.push_back(fn(a));
  return returnMe;
}

template<typename num>
inline num sum(const vector<num>* v) {
  num runningSum = 0;
  for(num n : *v) runningSum += n;
  return runningSum;
}

// note: will rearrange the input vector
template<typename num>
inline num median(vector<num>* v) {
  if(v->empty()) ERROR("Can't pass empty vector to median\n")
  if(v->size()==1) return v->front();
  nth_element(v->begin(), v->begin() + v->size()/2, v->end());
  return (*v)[v->size()/2];
}

template<typename num>
inline num min(const vector<num>* v) {
  if(v->empty()) ERROR("Can't pass empty vector to min\n")
  num runningMin = v->front();
  for(num n : *v) runningMin = MIN(runningMin, n);
  return runningMin;
}

template<typename num>
inline num max(const vector<num>* v) {
  if(v->empty()) ERROR("Can't pass empty vector to max\n")
  num runningMax = v->front();
  for(num n : *v) runningMax = MAX(runningMax, n);
  return runningMax;
}

template<typename num>
struct summary {
  num min;
  num firstQ;
  num median;
  num thirdQ;
  num max;
  num sum;
};

// note: will rearrange the input vector
template<typename num>
summary<num> summarize(vector<num>* v) {
  if(v->empty()) ERROR("Can't pass empty vector to summarize\n")
  summary<num> returnMe;
  returnMe.sum = sum(v);
  returnMe.median = median(v);
  if(v->size()==1) {
    returnMe.firstQ = v->front();
    returnMe.thirdQ = v->front();
    returnMe.min = v->front();
    returnMe.max = v->front();
  } else {
    vector<num> lowerHalf(v->cbegin(), v->cbegin() + v->size()/2);
    vector<num> upperHalf(v->cbegin() + v->size()/2, v->cend());
    returnMe.firstQ = median(&lowerHalf);
    returnMe.thirdQ = median(&upperHalf);
    returnMe.min = min(&lowerHalf);
    returnMe.max = max(&upperHalf);
  }
  return returnMe;
}

#endif
