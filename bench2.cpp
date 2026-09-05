#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
struct Clock { static double ms() {
    static LARGE_INTEGER f; static bool init=false;
    if(!init){ QueryPerformanceFrequency(&f); init=true; }
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}};
#else
#include <chrono>
using namespace std::chrono;
struct Clock { static double ms() {
    return (double)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()/1e6;
}};
#endif

#include <algorithm>
#include <vector>
#include <string>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <chrono>
#include <functional>
using namespace std;

#include "lxy_sort.hpp"

static mt19937 rng(20240);

// ============ timing (best-of-N) ============
template<typename T>
static double timeLxy(vector<T>& v, const char** alg=nullptr) {
    bool tiny = v.size()<1000;
    int reps = tiny ? 400 : 12;
    if (tiny) {
        // warmup 50 iters (thread_local init + branch-predictor), then average 400:
        // a single best is below QPC resolution (collapses to 0) and median is
        // skewed by first-call stalls; warmup+mean is accurate and unbiased.
        for (int r=0;r<50;r++){ vector<T> c=v;
            if (r==0 && alg) { const char* a=nullptr; lxySortTrace(c,&a); if(alg)*alg=a; }
            else lxySort(c); }
        double sum=0;
        for (int r=0;r<reps;r++){ vector<T> c=v; double s=Clock::ms(); lxySort(c); double e=Clock::ms(); sum+=e-s; }
        return sum/reps;
    }
    double best=1e300;
    for (int r=0;r<reps;r++){ vector<T> c=v;
        double s=Clock::ms();
        if (r==0 && alg) { const char* a=nullptr; lxySortTrace(c,&a); if(alg)*alg=a; }
        else lxySort(c);
        double e=Clock::ms();
        double t=e-s; if (t<best) best=t;
    }
    return best;
}
template<typename T>
static double timeStd(vector<T>& v) {
    bool tiny = v.size()<1000;
    int reps = tiny ? 400 : 12;
    if (tiny) {
        for (int r=0;r<50;r++){ vector<T> c=v; sort(c.begin(),c.end()); }
        double sum=0;
        for (int r=0;r<reps;r++){ vector<T> c=v; double s=Clock::ms(); sort(c.begin(),c.end()); double e=Clock::ms(); sum+=e-s; }
        return sum/reps;
    }
    double best=1e300;
    for (int r=0;r<reps;r++){ vector<T> c=v;
        double s=Clock::ms(); sort(c.begin(),c.end()); double e=Clock::ms();
        double t=e-s; if (t<best) best=t; }
    return best;
}
template<typename T, typename Cmp>
static double timeStdCmp(vector<T>& v, Cmp cmp) {
    bool tiny = v.size()<1000;
    int reps = tiny ? 400 : 12;
    if (tiny) {
        for (int r=0;r<50;r++){ vector<T> c=v; sort(c.begin(),c.end(),cmp); }
        double sum=0;
        for (int r=0;r<reps;r++){ vector<T> c=v; double s=Clock::ms(); sort(c.begin(),c.end(),cmp); double e=Clock::ms(); sum+=e-s; }
        return sum/reps;
    }
    double best=1e300;
    for (int r=0;r<reps;r++){ vector<T> c=v;
        double s=Clock::ms(); sort(c.begin(),c.end(),cmp); double e=Clock::ms();
        double t=e-s; if (t<best) best=t; }
    return best;
}

static int gWins=0, gTotal=0;

template<typename T>
static void runCase(const char* name, vector<T> v) {
    vector<T> ref=v; sort(ref.begin(), ref.end());
    const char* alg=nullptr;
    double t1=timeLxy(v,&alg);
    double t2=timeStd(v);
    vector<T> ck=v; lxySort(ck); bool ok=(ck==ref);
    bool win=t1<t2; gWins+=win; gTotal++;
    printf("[%s]\n", name);
    printf("  lxySort  : %10.6fms  [%s]\n", t1, alg?alg:"-");
    printf("  std::sort: %10.6fms\n", t2);
    printf("  -> %s  |  %.2fx %s\n", ok?"OK":"WRONG",
        (t1>0&&t2>0)?(win?(double)t2/t1:(double)t1/t2):0,
        win?"faster":"slower");
    printf("\n");
}
template<typename T, typename Cmp>
static void runCaseCmp(const char* name, vector<T> v, Cmp cmp) {
    vector<T> ref=v; sort(ref.begin(),ref.end(),cmp);
    vector<T> ck=v; lxySort(ck,cmp); bool ok=(ck==ref);
    double t1=timeLxy(v);
    double t2=timeStdCmp(v,cmp);
    bool win=t1<t2; gWins+=win; gTotal++;
    printf("[%s] (descending)\n", name);
    printf("  lxySort  : %8.3fms\n", t1);
    printf("  std::sort: %8.3fms\n", t2);
    printf("  -> %s  |  %.2fx %s\n\n", ok?"OK":"WRONG",
        (t1>0&&t2>0)?(win?(double)t2/t1:(double)t1/t2):0, win?"faster":"slower");
}

// ---- stability: lxyStableSort must preserve relative order of equal keys ----
struct BenchItem { int k, id; };
static void runStable(const char* name, vector<BenchItem> v) {
    auto byk=[](const BenchItem&x,const BenchItem&y){return x.k<y.k;};
    vector<BenchItem> ref=v; stable_sort(ref.begin(),ref.end(),byk);
    vector<BenchItem> ck=v; lxyStableSort(ck,byk);
    bool ok=true;
    for (size_t i=0;i<ref.size();i++) if (ck[i].k!=ref[i].k || ck[i].id!=ref[i].id){ok=false;break;}
    double t1=1e300,t2=1e300;
    int reps=v.size()<1000?300:8;
    {double b=1e300;for(int r=0;r<reps;r++){auto c=v;double s=Clock::ms();lxyStableSort(c,byk);double e=Clock::ms();if(e-s<b)b=e-s;}t1=b;}
    {double b=1e300;for(int r=0;r<reps;r++){auto c=v;double s=Clock::ms();stable_sort(c.begin(),c.end(),byk);double e=Clock::ms();if(e-s<b)b=e-s;}t2=b;}
    bool win=t1<t2; gWins+=win; gTotal++;
    printf("[%s] (stable)\n", name);
    printf("  lxyStableSort  : %8.3fms\n", t1);
    printf("  std::stable_sort: %7.3fms\n", t2);
    printf("  -> %s (stable OK)  |  %.2fx %s\n\n", ok?"STABLE":"UNSTABLE",
        (t1>0&&t2>0)?(win?(double)t2/t1:(double)t1/t2):0, win?"faster":"slower");
}

// ---- byKey: lxySortByKey(key-extractor) vs manual pair sort ----
template<typename T, typename KF>
static void runByKey(const char* name, vector<T> v, KF kf) {
    // reference: sort by (key, original index)
    vector<pair<long long,int>> pairs; pairs.reserve(v.size());
    for (int i=0;i<(int)v.size();i++) pairs.emplace_back((long long)kf(v[i]), i);
    stable_sort(pairs.begin(),pairs.end(),[](auto&x,auto&y){return x.first<y.first;});
    vector<T> out=v;
    lxySortByKey(out, kf, true);
    bool ok=true;
    for (size_t i=0;i<v.size();i++){
        // out[i] must equal the element whose (key,idx) is pairs[i]
        if (!(kf(out[i])==pairs[i].first)){ok=false;break;}
    }
    double t1=1e300;
    int reps=v.size()<1000?300:8;
    {double b=1e300;for(int r=0;r<reps;r++){auto c=v;double s=Clock::ms();lxySortByKey(c,kf,true);double e=Clock::ms();if(e-s<b)b=e-s;}t1=b;}
    printf("[%s] (byKey)\n", name);
    printf("  lxySortByKey : %8.3fms\n", t1);
    printf("  -> %s\n\n", ok?"OK":"WRONG");
}

int main(int argc, char** argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 100000;
    printf("=== lxySort  vs  std::sort  (N=%d) ===\n\n", N);

    // ================= int =================
    printf("---------- int ----------\n\n");
    auto R_wide  =[&](int n){vector<int>a(n);for(auto&x:a)x=(int)(rng()%1000000000);return a;};       // 0..1e9
    auto R_999999=[&](int n){vector<int>a(n);for(auto&x:a)x=rng()%1000000;return a;};                 // 0..999999
    auto R_999   =[&](int n){vector<int>a(n);for(auto&x:a)x=rng()%1000;return a;};                    // 0..999
    auto R_99    =[&](int n){vector<int>a(n);for(auto&x:a)x=rng()%100;return a;};                     // 0..99
    auto R_9     =[&](int n){vector<int>a(n);for(auto&x:a)x=rng()%10;return a;};                      // 0..9 many dup
    auto R_neg   =[&](int n){vector<int>a(n);for(auto&x:a)x=(int)(rng()%200000-100000);return a;};    // -100k..100k
    auto R_mix   =[&](int n){vector<int>a(n);for(auto&x:a)x=(int)(rng()%2000000000u-1000000000);return a;}; // mixed signs
    auto ASC     =[](int n){vector<int>a(n);for(int i=0;i<n;i++)a[i]=i;return a;};                    // ascending
    auto ASCrep  =[&](int n){vector<int>a(n);for(int i=0;i<n;i++)a[i]=i/4;return a;};                 // ascending+repeats
    auto DESC    =[](int n){vector<int>a(n);for(int i=0;i<n;i++)a[i]=n-i;return a;};                  // descending
    auto DESCrep =[&](int n){vector<int>a(n);for(int i=0;i<n;i++)a[i]=(n-i)/4;return a;};             // descending+repeats
    auto SAME    =[](int n){return vector<int>(n,42);};                                               /
