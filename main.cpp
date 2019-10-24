#include <iostream>
#include <string>
using namespace std;

extern "C"
{
    int maxofthree(int, int, int);
}

int main()
{
    cout << maxofthree(1, -4, -7) << endl;
    cout << maxofthree(2, -6, 1) << endl;
    cout << maxofthree(2, 3, 1) << endl;
    cout << maxofthree(-2, 4, 3) << endl;
    cout << maxofthree(2, -6, 5) << endl;
    cout << maxofthree(2, 4, 6) << endl;
    return 0;
}