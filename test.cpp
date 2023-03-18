#include <iostream>
using namespace std;

constexpr int display(int x) {
    int ret = 1 + 2 + x;
    return ret;
}

int main()
{
   display(1);
    return 0;
}
